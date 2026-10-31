namespace Chess.Logic
{
    // A deliberately modest opponent: fixed-depth alpha-beta over material and
    // piece-square tables, with a capture-only quiescence search so it doesn't
    // hang pieces at the horizon, and a little noise so games vary.
    public sealed class Bot
    {
        private const int MateScore = 100000;
        private const int Infinity = 1000000;

        private static readonly int[] s_PieceValues = { 0, 100, 320, 330, 500, 900, 0 };

        // Tables are written from White's point of view with rank 8 on top, as
        // they'd look on a diagram. Values from the "simplified evaluation function".
        private static readonly int[][] s_Tables =
        {
            new int[64],
            new[]
            {
                 0,  0,  0,  0,  0,  0,  0,  0,
                50, 50, 50, 50, 50, 50, 50, 50,
                10, 10, 20, 30, 30, 20, 10, 10,
                 5,  5, 10, 25, 25, 10,  5,  5,
                 0,  0,  0, 20, 20,  0,  0,  0,
                 5, -5,-10,  0,  0,-10, -5,  5,
                 5, 10, 10,-20,-20, 10, 10,  5,
                 0,  0,  0,  0,  0,  0,  0,  0,
            },
            new[]
            {
                -50,-40,-30,-30,-30,-30,-40,-50,
                -40,-20,  0,  0,  0,  0,-20,-40,
                -30,  0, 10, 15, 15, 10,  0,-30,
                -30,  5, 15, 20, 20, 15,  5,-30,
                -30,  0, 15, 20, 20, 15,  0,-30,
                -30,  5, 10, 15, 15, 10,  5,-30,
                -40,-20,  0,  5,  5,  0,-20,-40,
                -50,-40,-30,-30,-30,-30,-40,-50,
            },
            new[]
            {
                -20,-10,-10,-10,-10,-10,-10,-20,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -10,  0,  5, 10, 10,  5,  0,-10,
                -10,  5,  5, 10, 10,  5,  5,-10,
                -10,  0, 10, 10, 10, 10,  0,-10,
                -10, 10, 10, 10, 10, 10, 10,-10,
                -10,  5,  0,  0,  0,  0,  5,-10,
                -20,-10,-10,-10,-10,-10,-10,-20,
            },
            new[]
            {
                 0,  0,  0,  0,  0,  0,  0,  0,
                 5, 10, 10, 10, 10, 10, 10,  5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                 0,  0,  0,  5,  5,  0,  0,  0,
            },
            new[]
            {
                -20,-10,-10, -5, -5,-10,-10,-20,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -10,  0,  5,  5,  5,  5,  0,-10,
                 -5,  0,  5,  5,  5,  5,  0, -5,
                  0,  0,  5,  5,  5,  5,  0, -5,
                -10,  5,  5,  5,  5,  5,  0,-10,
                -10,  0,  5,  0,  0,  0,  0,-10,
                -20,-10,-10, -5, -5,-10,-10,-20,
            },
            new[]
            {
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -20,-30,-30,-40,-40,-30,-30,-20,
                -10,-20,-20,-20,-20,-20,-20,-10,
                 20, 20,  0,  0,  0,  0, 20, 20,
                 20, 30, 10,  0,  0, 10, 30, 20,
            },
        };

        public int Depth { get; set; } = 3;

        // Centipawns of random noise added to each root move; keeps the bot from
        // replaying the same game while barely affecting its strength.
        public int Noise { get; set; } = 12;

        private readonly Random m_Random;

        public Bot(int? seed = null)
        {
            m_Random = seed.HasValue ? new Random(seed.Value) : new Random();
        }

        public Move? ChooseMove(Board position)
        {
            Board board = position.Clone();
            List<Move> moves = MoveGenerator.GenerateLegal(board);
            if (moves.Count == 0)
                return null;

            Order(board, moves);

            Move best = moves[0];
            int bestScore = -Infinity;
            foreach (Move move in moves)
            {
                Undo undo = board.MakeMove(move);
                int score = -Search(board, Depth - 1, 1, -Infinity, Infinity) + m_Random.Next(Noise + 1);
                board.UnmakeMove(move, undo);

                if (score > bestScore)
                {
                    bestScore = score;
                    best = move;
                }
            }
            return best;
        }

        private int Search(Board board, int depth, int ply, int alpha, int beta)
        {
            if (depth == 0)
                return Quiesce(board, alpha, beta, 4);

            List<Move> moves = MoveGenerator.GenerateLegal(board);
            if (moves.Count == 0)
                return board.InCheck(board.SideToMove) ? -MateScore + ply : 0;
            if (board.HalfmoveClock >= 100)
                return 0;

            Order(board, moves);
            foreach (Move move in moves)
            {
                Undo undo = board.MakeMove(move);
                int score = -Search(board, depth - 1, ply + 1, -beta, -alpha);
                board.UnmakeMove(move, undo);

                if (score >= beta)
                    return beta;
                if (score > alpha)
                    alpha = score;
            }
            return alpha;
        }

        private int Quiesce(Board board, int alpha, int beta, int depthLeft)
        {
            int standPat = Evaluate(board);
            if (standPat >= beta || depthLeft == 0)
                return standPat;
            if (standPat > alpha)
                alpha = standPat;

            List<Move> captures = MoveGenerator.GenerateLegal(board, capturesOnly: true);
            Order(board, captures);
            foreach (Move move in captures)
            {
                Undo undo = board.MakeMove(move);
                int score = -Quiesce(board, -beta, -alpha, depthLeft - 1);
                board.UnmakeMove(move, undo);

                if (score >= beta)
                    return beta;
                if (score > alpha)
                    alpha = score;
            }
            return alpha;
        }

        // Score from the point of view of the side to move.
        private static int Evaluate(Board board)
        {
            int score = 0;
            for (int square = 0; square < 64; square++)
            {
                int piece = board.Squares[square];
                if (piece == Piece.None)
                    continue;

                int type = (int)Piece.TypeOf(piece);
                int file = Square.FileOf(square);
                int rank = Square.RankOf(square);
                bool white = Piece.ColorOf(piece) == PieceColor.White;
                int tableIndex = white ? (7 - rank) * 8 + file : rank * 8 + file;
                int value = s_PieceValues[type] + s_Tables[type][tableIndex];
                score += white ? value : -value;
            }
            return board.SideToMove == PieceColor.White ? score : -score;
        }

        // Captures first, most valuable victim / least valuable attacker; then promotions.
        private static void Order(Board board, List<Move> moves)
        {
            int Key(Move move)
            {
                int key = 0;
                if (move.IsCapture)
                {
                    int victim = move.IsEnPassant ? (int)PieceType.Pawn : (int)Piece.TypeOf(board.Squares[move.To]);
                    int attacker = (int)Piece.TypeOf(board.Squares[move.From]);
                    key += 1000 + s_PieceValues[victim] - attacker;
                }
                if (move.IsPromotion)
                    key += s_PieceValues[(int)move.Promotion];
                return key;
            }

            moves.Sort((a, b) => Key(b).CompareTo(Key(a)));
        }
    }
}
