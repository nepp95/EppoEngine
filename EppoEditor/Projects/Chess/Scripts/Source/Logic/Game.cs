namespace Chess.Logic
{
    public enum GameResult
    {
        Ongoing,
        WhiteWins,
        BlackWins,
        Draw,
    }

    // A game in progress: the board plus the history the draw rules need.
    public sealed class Game
    {
        public Board Board { get; private set; }
        public GameResult Result { get; private set; }
        public string ResultReason { get; private set; } = "";
        public IReadOnlyList<Move> LegalMoves => m_LegalMoves;
        public Move? LastMove { get; private set; }

        private List<Move> m_LegalMoves = new();
        private readonly Dictionary<string, int> m_Repetitions = new();

        public Game(string fen = Board.StartFen)
        {
            Board = Board.FromFen(fen);
            Record();
            UpdateStatus();
        }

        public IEnumerable<Move> LegalMovesFrom(int square)
            => m_LegalMoves.Where(m => m.From == square);

        // Finds the legal move between two squares. Promotions default to a queen.
        public Move? FindMove(int from, int to, PieceType promotion = PieceType.Queen)
        {
            foreach (Move move in m_LegalMoves)
            {
                if (move.From == from && move.To == to && (!move.IsPromotion || move.Promotion == promotion))
                    return move;
            }
            return null;
        }

        public void Play(Move move)
        {
            if (Result != GameResult.Ongoing)
                throw new InvalidOperationException("The game is over");
            if (!m_LegalMoves.Contains(move))
                throw new ArgumentException($"Illegal move {move}");

            Board.MakeMove(move);
            LastMove = move;
            Record();
            UpdateStatus();
        }

        private void Record()
        {
            string key = Board.PositionKey();
            m_Repetitions[key] = m_Repetitions.TryGetValue(key, out int count) ? count + 1 : 1;
        }

        private void UpdateStatus()
        {
            m_LegalMoves = MoveGenerator.GenerateLegal(Board);

            if (m_LegalMoves.Count == 0)
            {
                if (Board.InCheck(Board.SideToMove))
                {
                    Result = Board.SideToMove == PieceColor.White ? GameResult.BlackWins : GameResult.WhiteWins;
                    ResultReason = "checkmate";
                }
                else
                {
                    Result = GameResult.Draw;
                    ResultReason = "stalemate";
                }
            }
            else if (Board.HalfmoveClock >= 100)
            {
                Result = GameResult.Draw;
                ResultReason = "fifty-move rule";
            }
            else if (m_Repetitions[Board.PositionKey()] >= 3)
            {
                Result = GameResult.Draw;
                ResultReason = "threefold repetition";
            }
            else if (IsInsufficientMaterial(Board))
            {
                Result = GameResult.Draw;
                ResultReason = "insufficient material";
            }
        }

        // King vs king, or king and a single minor piece vs king.
        private static bool IsInsufficientMaterial(Board board)
        {
            int minors = 0;
            foreach (int piece in board.Squares)
            {
                switch (Piece.TypeOf(piece))
                {
                    case PieceType.None:
                    case PieceType.King:
                        break;
                    case PieceType.Knight:
                    case PieceType.Bishop:
                        minors++;
                        break;
                    default:
                        return false;
                }
            }
            return minors <= 1;
        }
    }
}
