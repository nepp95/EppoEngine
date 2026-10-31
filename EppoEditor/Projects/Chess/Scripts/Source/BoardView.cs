using Chess.Logic;
using EppoScriptCore.Math;
using EppoScriptCore.Scene;

namespace Chess
{
    // Owns every entity that shows the game: tiles, pieces and highlight markers.
    // Pieces are plain spawned entities moved around by tweens; nothing here knows
    // the rules beyond what a Move describes.
    public sealed class BoardView
    {
        // Spawned entities can't be hidden, so unused markers wait under the table.
        private static readonly Vector3 s_HiddenPosition = new(0.0f, -30.0f, 0.0f);

        private const float MoveSeconds = 0.35f;
        private const float CaptureSeconds = 0.45f;

        private readonly List<Entity> m_Spawned = new();
        private readonly Dictionary<int, Entity> m_Pieces = new();
        private readonly List<Tween> m_Tweens = new();

        private readonly Entity m_Hover;
        private readonly Entity m_Selected;
        private readonly Entity m_Check;
        private readonly Entity[] m_LastMove = new Entity[2];
        private readonly List<Entity> m_MoveDots = new();
        private readonly List<Entity> m_CaptureRings = new();
        private readonly List<Entity> m_DrawRings = new();

        private int m_CapturedWhite;
        private int m_CapturedBlack;

        public bool IsAnimating => m_Tweens.Count > 0;

        public BoardView()
        {
            for (int square = 0; square < 64; square++)
            {
                bool dark = (Square.FileOf(square) + Square.RankOf(square)) % 2 == 0;
                Spawn($"Square {Square.Name(square)}", dark ? MeshHandles.SquareDark : MeshHandles.SquareLight, SquareToWorld(square));
            }

            m_Hover = Spawn("Marker Hover", MeshHandles.MarkerHover, s_HiddenPosition);
            m_Selected = Spawn("Marker Selected", MeshHandles.MarkerSelected, s_HiddenPosition);
            m_Check = Spawn("Marker Check", MeshHandles.MarkerCheck, s_HiddenPosition);
            m_LastMove[0] = Spawn("Marker Last Move", MeshHandles.MarkerLastMove, s_HiddenPosition);
            m_LastMove[1] = Spawn("Marker Last Move", MeshHandles.MarkerLastMove, s_HiddenPosition);
        }

        public static Vector3 SquareToWorld(int square)
            => new(Square.FileOf(square) - 3.5f, 0.0f, 3.5f - Square.RankOf(square));

        public static int WorldToSquare(Vector3 position)
        {
            int file = (int)MathF.Floor(position.X + 4.0f);
            int rank = (int)MathF.Floor(4.0f - position.Z);
            return Square.OnBoard(file, rank) ? Square.At(file, rank) : Square.None;
        }

        // Rebuilds every piece from scratch; used at the start of each game.
        public void SetPosition(Board board)
        {
            foreach (Entity piece in m_Pieces.Values)
                Destroy(piece);
            m_Pieces.Clear();
            m_Tweens.Clear();
            m_CapturedWhite = 0;
            m_CapturedBlack = 0;

            for (int square = 0; square < 64; square++)
            {
                int piece = board.Squares[square];
                if (piece != Piece.None)
                    m_Pieces[square] = SpawnPiece(piece, square);
            }

            ClearHighlights();
            ShowLastMove(null);
            ShowCheck(Square.None);
        }

        public bool HasPiece(int square) => m_Pieces.ContainsKey(square);

        // Mirrors a move the board has already made: slides the piece, retires any
        // capture to the side of the board, moves the castling rook and swaps the
        // mesh on promotion.
        public void ApplyMove(Move move, int movedPiece, int capturedPiece)
        {
            if (move.IsCapture)
            {
                int capturedSquare = move.IsEnPassant ? Square.At(Square.FileOf(move.To), Square.RankOf(move.From)) : move.To;
                if (m_Pieces.Remove(capturedSquare, out var captured))
                    m_Tweens.Add(new Tween(captured, GraveyardSlot(capturedPiece), CaptureSeconds, 1.2f, 0.65f));
            }

            if (m_Pieces.Remove(move.From, out var mover))
            {
                m_Pieces[move.To] = mover;
                float lift = Piece.TypeOf(movedPiece) == PieceType.Knight ? 0.8f : 0.25f;
                m_Tweens.Add(new Tween(mover, SquareToWorld(move.To), MoveSeconds, lift));

                if (move.IsPromotion)
                    mover.GetComponent<MeshComponent>()!.MeshHandle = MeshFor(Piece.Make(Piece.ColorOf(movedPiece), move.Promotion));
            }

            if (move.IsCastle)
            {
                bool kingside = move.To > move.From;
                int rookFrom = kingside ? move.From + 3 : move.From - 4;
                int rookTo = kingside ? move.From + 1 : move.From - 1;
                if (m_Pieces.Remove(rookFrom, out var rook))
                {
                    m_Pieces[rookTo] = rook;
                    m_Tweens.Add(new Tween(rook, SquareToWorld(rookTo), MoveSeconds, 0.6f));
                }
            }
        }

        public void Update(float timestep)
        {
            for (int i = m_Tweens.Count - 1; i >= 0; i--)
            {
                if (m_Tweens[i].Advance(timestep))
                    m_Tweens.RemoveAt(i);
            }
        }

        public void ShowHover(int square) => Place(m_Hover, square);

        public void ShowSelection(int square, IEnumerable<Move> targets)
        {
            ClearHighlights();
            Place(m_Selected, square);

            int dots = 0, rings = 0;
            foreach (Move move in targets)
            {
                // Promotions list one move per piece type; mark the square once.
                if (move.IsPromotion && move.Promotion != PieceType.Queen)
                    continue;

                if (move.IsCapture)
                    Place(Pooled(m_CaptureRings, rings++, MeshHandles.MarkerCapture), move.To);
                else
                    Place(Pooled(m_MoveDots, dots++, MeshHandles.MarkerMove), move.To);
            }
        }

        public void ClearHighlights()
        {
            Place(m_Selected, Square.None);
            foreach (Entity dot in m_MoveDots)
                Place(dot, Square.None);
            foreach (Entity ring in m_CaptureRings)
                Place(ring, Square.None);
        }

        public void ShowLastMove(Move? move)
        {
            Place(m_LastMove[0], move?.From ?? Square.None);
            Place(m_LastMove[1], move?.To ?? Square.None);
        }

        public void ShowCheck(int kingSquare) => Place(m_Check, kingSquare);

        public void ShowDraw(Board board)
        {
            Place(Pooled(m_DrawRings, 0, MeshHandles.MarkerHover), board.KingSquare(PieceColor.White));
            Place(Pooled(m_DrawRings, 1, MeshHandles.MarkerHover), board.KingSquare(PieceColor.Black));
        }

        public void HideDraw()
        {
            foreach (Entity ring in m_DrawRings)
                Place(ring, Square.None);
        }

        // Lays the defeated king on its side.
        public void TopplePiece(int square)
        {
            if (!m_Pieces.TryGetValue(square, out var king))
                return;

            // Rolling +90 degrees about Z lays the piece along -X, so shift it back
            // over the square and up by its base radius.
            Vector3 rest = SquareToWorld(square) + new Vector3(0.45f, 0.3f, 0.0f);
            m_Tweens.Add(new Tween(king, rest, 0.6f, 0.0f, 1.0f, new Vector3(0.0f, 0.0f, MathF.PI / 2.0f)));
        }

        // Ray-picks the square under the cursor. Pieces are tested as upright
        // cylinders first, so clicking a tall piece's head selects its square rather
        // than the one behind it; otherwise the ray hits the board plane.
        public int Pick(Vector3 origin, Vector3 direction)
        {
            float bestT = float.MaxValue;
            int bestSquare = Square.None;

            foreach ((int square, Entity piece) in m_Pieces)
            {
                Vector3 center = SquareToWorld(square);
                if (RayCylinder(origin, direction, center, 0.3f, 0.85f, out float t) && t < bestT)
                {
                    bestT = t;
                    bestSquare = square;
                }
            }

            if (bestSquare != Square.None)
                return bestSquare;

            if (direction.Y > -1e-4f)
                return Square.None;

            float planeT = -origin.Y / direction.Y;
            return WorldToSquare(origin + direction * planeT);
        }

        public void Destroy()
        {
            foreach (Entity entity in m_Spawned)
                Scene.DestroyEntity(entity);
            m_Spawned.Clear();
            m_Pieces.Clear();
            m_Tweens.Clear();
        }

        private static bool RayCylinder(Vector3 o, Vector3 d, Vector3 c, float radius, float height, out float hitT)
        {
            hitT = float.MaxValue;
            float ox = o.X - c.X, oz = o.Z - c.Z;

            float a = d.X * d.X + d.Z * d.Z;
            float b = 2.0f * (ox * d.X + oz * d.Z);
            float k = ox * ox + oz * oz - radius * radius;
            float disc = b * b - 4.0f * a * k;
            if (a > 1e-8f && disc >= 0.0f)
            {
                float t = (-b - MathF.Sqrt(disc)) / (2.0f * a);
                float y = o.Y + d.Y * t;
                if (t > 0.0f && y >= 0.0f && y <= height)
                    hitT = t;
            }

            // Top cap, for rays looking steeply down onto the piece.
            if (MathF.Abs(d.Y) > 1e-8f)
            {
                float t = (height - o.Y) / d.Y;
                float x = ox + d.X * t, z = oz + d.Z * t;
                if (t > 0.0f && x * x + z * z <= radius * radius && t < hitT)
                    hitT = t;
            }

            return hitT < float.MaxValue;
        }

        private Vector3 GraveyardSlot(int piece)
        {
            // White's losses line up on Black's left, Black's on White's right.
            bool white = Piece.ColorOf(piece) == PieceColor.White;
            int index = white ? m_CapturedWhite++ : m_CapturedBlack++;
            float x = white ? -5.4f - (index / 8) * 0.7f : 5.4f + (index / 8) * 0.7f;
            float z = white ? -3.5f + (index % 8) : 3.5f - (index % 8);
            return new Vector3(x, -0.42f, z);
        }

        private Entity SpawnPiece(int piece, int square)
        {
            Entity entity = Spawn(PieceName(piece), MeshFor(piece), SquareToWorld(square));

            // Meshes face -Z (towards Black); turn Black's army around.
            if (Piece.ColorOf(piece) == PieceColor.Black)
                entity.GetComponent<TransformComponent>()!.Rotation = new Vector3(0.0f, MathF.PI, 0.0f);
            return entity;
        }

        private Entity Spawn(string name, ulong mesh, Vector3 position)
        {
            Entity entity = Scene.CreateEntity(name);
            entity.AddComponent<MeshComponent>().MeshHandle = mesh;
            entity.Translation = position;
            m_Spawned.Add(entity);
            return entity;
        }

        private void Destroy(Entity entity)
        {
            m_Spawned.Remove(entity);
            Scene.DestroyEntity(entity);
        }

        private Entity Pooled(List<Entity> pool, int index, ulong mesh)
        {
            while (pool.Count <= index)
                pool.Add(Spawn("Marker", mesh, s_HiddenPosition));
            return pool[index];
        }

        private static void Place(Entity marker, int square)
            => marker.Translation = square == Square.None ? s_HiddenPosition : SquareToWorld(square);

        private static string PieceName(int piece)
            => $"{Piece.ColorOf(piece)} {Piece.TypeOf(piece)}";

        private static ulong MeshFor(int piece)
        {
            bool white = Piece.ColorOf(piece) == PieceColor.White;
            return Piece.TypeOf(piece) switch
            {
                PieceType.Pawn => white ? MeshHandles.WhitePawn : MeshHandles.BlackPawn,
                PieceType.Knight => white ? MeshHandles.WhiteKnight : MeshHandles.BlackKnight,
                PieceType.Bishop => white ? MeshHandles.WhiteBishop : MeshHandles.BlackBishop,
                PieceType.Rook => white ? MeshHandles.WhiteRook : MeshHandles.BlackRook,
                PieceType.Queen => white ? MeshHandles.WhiteQueen : MeshHandles.BlackQueen,
                _ => white ? MeshHandles.WhiteKing : MeshHandles.BlackKing,
            };
        }
    }
}
