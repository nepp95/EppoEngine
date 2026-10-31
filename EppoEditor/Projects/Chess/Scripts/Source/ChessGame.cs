using Chess.Logic;
using EppoScriptCore.Core;
using EppoScriptCore.Math;
using EppoScriptCore.Scene;

namespace Chess
{
    // Drives the whole game: mouse input for the player, the bot on a worker
    // thread, and the BoardView that mirrors the rules state in the scene.
    //
    // Controls: left click a piece, then a highlighted square to move it.
    // Right click cancels the selection, R starts a new game.
    public class ChessGame : Entity
    {
        public bool PlayerIsWhite = true;
        public int BotDepth = 3;
        public float BotDelay = 0.4f;

        private static readonly Vector3 s_WinColor = new(0.45f, 1.0f, 0.5f);
        private static readonly Vector3 s_LossColor = new(1.0f, 0.3f, 0.25f);

        private Game m_Game = new();
        private BoardView m_View = null!;
        private CameraComponent? m_Camera;
        private PointLightComponent? m_MoodLight;
        private Vector3 m_MoodLightColor;

        private int m_Selected = Square.None;
        private bool m_WasLeftDown;
        private bool m_WasRightDown;
        private bool m_WasRestartDown;

        private Bot m_Bot = new();
        private Task<Move?>? m_BotTask;
        private int m_GameNumber;
        private float m_BotWait;
        private bool m_ResultShown;

        private PieceColor PlayerColor => PlayerIsWhite ? PieceColor.White : PieceColor.Black;
        private bool IsPlayersTurn => m_Game.Result == GameResult.Ongoing && m_Game.Board.SideToMove == PlayerColor;

        public override void OnCreate()
        {
            // The editor captures the cursor for play mode; this game needs to see it.
            Input.CursorMode = CursorMode.Normal;

            m_Camera = Scene.FindEntityByName("Camera")?.GetComponent<CameraComponent>();
            if (m_Camera == null)
                Log.Error("Chess: no entity named 'Camera' with a CameraComponent; mouse picking is disabled");
            else if (!PlayerIsWhite)
                SeatCameraForBlack(m_Camera.Entity);

            m_MoodLight = Scene.FindEntityByName("Fill Light White")?.GetComponent<PointLightComponent>();
            if (m_MoodLight != null)
                m_MoodLightColor = m_MoodLight.Color;

            m_View = new BoardView();
            NewGame();
        }

        public override void OnUpdate(float timestep)
        {
            m_View.Update(timestep);

            if (Pressed(Input.IsKeyPressed(KeyCode.R), ref m_WasRestartDown))
            {
                NewGame();
                return;
            }

            bool leftClick = Pressed(Input.IsMouseButtonPressed(MouseCode.ButtonLeft), ref m_WasLeftDown);
            bool rightClick = Pressed(Input.IsMouseButtonPressed(MouseCode.ButtonRight), ref m_WasRightDown);

            if (m_View.IsAnimating)
                return;

            if (m_Game.Result != GameResult.Ongoing)
            {
                ShowResult();
                return;
            }

            if (IsPlayersTurn)
                UpdatePlayer(leftClick, rightClick);
            else
                UpdateBot(timestep);
        }

        public override void OnDestroy()
        {
            // A search still running finishes on its own; its result is dropped.
            m_BotTask = null;
            m_View?.Destroy();
        }

        private void NewGame()
        {
            m_GameNumber++;
            m_Game = new Game();
            m_Bot = new Bot { Depth = Math.Clamp(BotDepth, 1, 5) };
            m_BotTask = null;
            m_Selected = Square.None;
            m_ResultShown = false;

            m_View.SetPosition(m_Game.Board);
            m_View.HideDraw();
            if (m_MoodLight != null)
                m_MoodLight.Color = m_MoodLightColor;

            Log.Info($"Chess: new game #{m_GameNumber}, you play {PlayerColor}. Left click to move, right click to cancel, R to restart.");
        }

        private void UpdatePlayer(bool leftClick, bool rightClick)
        {
            int hovered = PickSquare();
            m_View.ShowHover(hovered);

            if (rightClick)
            {
                Deselect();
                return;
            }

            if (!leftClick)
                return;

            if (m_Selected != Square.None && hovered != Square.None)
            {
                Move? move = m_Game.FindMove(m_Selected, hovered);
                if (move.HasValue)
                {
                    m_View.ShowHover(Square.None);
                    Play(move.Value);
                    return;
                }
            }

            if (hovered != Square.None && hovered != m_Selected && Piece.IsColor(m_Game.Board.Squares[hovered], PlayerColor))
            {
                m_Selected = hovered;
                m_View.ShowSelection(hovered, m_Game.LegalMovesFrom(hovered));
            }
            else
            {
                Deselect();
            }
        }

        private void UpdateBot(float timestep)
        {
            if (m_BotTask == null)
            {
                // Search a copy on a worker thread so a slow move never stalls a frame.
                // NewGame drops the task, so a stale result never reaches a new game.
                Board snapshot = m_Game.Board.Clone();
                Bot bot = m_Bot;
                m_BotTask = Task.Run(() => bot.ChooseMove(snapshot));
                m_BotWait = BotDelay;
                return;
            }

            // Hold the reply back a moment so the bot doesn't move instantly.
            m_BotWait -= timestep;
            if (m_BotWait > 0.0f || !m_BotTask.IsCompleted)
                return;

            Task<Move?> task = m_BotTask;
            m_BotTask = null;

            if (task.IsFaulted)
                Log.Error($"Chess: the bot crashed: {task.Exception?.GetBaseException().Message}");
            else if (task.Result.HasValue)
                Play(task.Result.Value);
        }

        private void Play(Move move)
        {
            Board board = m_Game.Board;
            int moved = board.Squares[move.From];
            int captured = move.IsEnPassant
                ? board.Squares[Square.At(Square.FileOf(move.To), Square.RankOf(move.From))]
                : board.Squares[move.To];
            PieceColor mover = board.SideToMove;

            m_Game.Play(move);
            Deselect();

            m_View.ApplyMove(move, moved, captured);
            m_View.ShowLastMove(move);

            PieceColor toMove = m_Game.Board.SideToMove;
            bool check = m_Game.Board.InCheck(toMove);
            m_View.ShowCheck(check ? m_Game.Board.KingSquare(toMove) : Square.None);

            string who = mover == PlayerColor ? "You" : "Bot";
            Log.Info($"Chess: {who} played {move}{(check ? " +" : "")}");
        }

        private void ShowResult()
        {
            if (m_ResultShown)
                return;
            m_ResultShown = true;

            Board board = m_Game.Board;
            switch (m_Game.Result)
            {
                case GameResult.WhiteWins:
                case GameResult.BlackWins:
                {
                    PieceColor winner = m_Game.Result == GameResult.WhiteWins ? PieceColor.White : PieceColor.Black;
                    m_View.TopplePiece(board.KingSquare(Piece.Opponent(winner)));

                    bool playerWon = winner == PlayerColor;
                    if (m_MoodLight != null)
                        m_MoodLight.Color = playerWon ? s_WinColor : s_LossColor;
                    Log.Info(playerWon
                        ? $"Chess: you win by {m_Game.ResultReason}! Press R to play again."
                        : $"Chess: the bot wins by {m_Game.ResultReason}. Press R to play again.");
                    break;
                }

                case GameResult.Draw:
                {
                    m_View.ShowDraw(board);
                    Log.Info($"Chess: draw by {m_Game.ResultReason}. Press R to play again.");
                    break;
                }
            }
        }

        private int PickSquare()
        {
            if (m_Camera == null)
                return Square.None;

            Vector2 mouse = Input.GetViewportMousePosition();
            Vector2 size = Input.GetViewportSize();
            if (mouse.X < 0.0f || mouse.Y < 0.0f || mouse.X >= size.X || mouse.Y >= size.Y)
                return Square.None;

            if (!m_Camera.ScreenPointToRay(mouse, out Vector3 origin, out Vector3 direction))
                return Square.None;

            return m_View.Pick(origin, direction);
        }

        private void Deselect()
        {
            m_Selected = Square.None;
            m_View.ClearHighlights();
        }

        // Mirrors the scene camera to Black's side of the board.
        private static void SeatCameraForBlack(Entity camera)
        {
            var transform = camera.GetComponent<TransformComponent>()!;
            Vector3 position = transform.Translation;
            transform.Translation = new Vector3(-position.X, position.Y, -position.Z);
            transform.Rotation = new Vector3(transform.Rotation.X, transform.Rotation.Y + MathF.PI, transform.Rotation.Z);
        }

        // True on the frame a button goes down; polling only reports held state.
        private static bool Pressed(bool down, ref bool wasDown)
        {
            bool pressed = down && !wasDown;
            wasDown = down;
            return pressed;
        }
    }
}
