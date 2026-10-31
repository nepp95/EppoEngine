namespace Chess.Logic
{
    public static class MoveGenerator
    {
        private static readonly (int, int)[] s_KnightSteps = { (1, 2), (2, 1), (2, -1), (1, -2), (-1, -2), (-2, -1), (-2, 1), (-1, 2) };
        private static readonly (int, int)[] s_KingSteps = { (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1) };
        private static readonly (int, int)[] s_RookDirections = { (1, 0), (-1, 0), (0, 1), (0, -1) };
        private static readonly (int, int)[] s_BishopDirections = { (1, 1), (1, -1), (-1, 1), (-1, -1) };

        private static readonly PieceType[] s_Promotions = { PieceType.Queen, PieceType.Rook, PieceType.Bishop, PieceType.Knight };

        public static List<Move> GenerateLegal(Board board, bool capturesOnly = false)
        {
            var pseudo = new List<Move>(64);
            GeneratePseudoLegal(board, pseudo, capturesOnly);

            PieceColor us = board.SideToMove;
            var legal = new List<Move>(pseudo.Count);
            foreach (Move move in pseudo)
            {
                Undo undo = board.MakeMove(move);
                if (!board.InCheck(us))
                    legal.Add(move);
                board.UnmakeMove(move, undo);
            }
            return legal;
        }

        public static void GeneratePseudoLegal(Board board, List<Move> moves, bool capturesOnly = false)
        {
            PieceColor us = board.SideToMove;
            for (int square = 0; square < 64; square++)
            {
                int piece = board.Squares[square];
                if (!Piece.IsColor(piece, us))
                    continue;

                switch (Piece.TypeOf(piece))
                {
                    case PieceType.Pawn:
                        AddPawnMoves(board, square, us, moves, capturesOnly);
                        break;
                    case PieceType.Knight:
                        AddSteps(board, square, us, s_KnightSteps, moves, capturesOnly);
                        break;
                    case PieceType.Bishop:
                        AddSlides(board, square, us, s_BishopDirections, moves, capturesOnly);
                        break;
                    case PieceType.Rook:
                        AddSlides(board, square, us, s_RookDirections, moves, capturesOnly);
                        break;
                    case PieceType.Queen:
                        AddSlides(board, square, us, s_RookDirections, moves, capturesOnly);
                        AddSlides(board, square, us, s_BishopDirections, moves, capturesOnly);
                        break;
                    case PieceType.King:
                        AddSteps(board, square, us, s_KingSteps, moves, capturesOnly);
                        if (!capturesOnly)
                            AddCastling(board, square, us, moves);
                        break;
                }
            }
        }

        private static void AddPawnMoves(Board board, int from, PieceColor us, List<Move> moves, bool capturesOnly)
        {
            int file = Square.FileOf(from);
            int rank = Square.RankOf(from);
            int dir = us == PieceColor.White ? 1 : -1;
            int startRank = us == PieceColor.White ? 1 : 6;
            int lastRank = us == PieceColor.White ? 7 : 0;
            int nextRank = rank + dir;

            if (!capturesOnly && board.Squares[Square.At(file, nextRank)] == Piece.None)
            {
                AddPawnMove(from, Square.At(file, nextRank), MoveFlags.None, nextRank == lastRank, moves);

                int doubleTo = Square.At(file, rank + 2 * dir);
                if (rank == startRank && board.Squares[doubleTo] == Piece.None)
                    moves.Add(new Move(from, doubleTo, MoveFlags.DoublePush));
            }

            for (int df = -1; df <= 1; df += 2)
            {
                if (!Square.OnBoard(file + df, nextRank))
                    continue;

                int to = Square.At(file + df, nextRank);
                int target = board.Squares[to];
                if (target != Piece.None && Piece.ColorOf(target) != us)
                    AddPawnMove(from, to, MoveFlags.Capture, nextRank == lastRank, moves);
                else if (to == board.EnPassantSquare)
                    moves.Add(new Move(from, to, MoveFlags.Capture | MoveFlags.EnPassant));
            }
        }

        private static void AddPawnMove(int from, int to, MoveFlags flags, bool promotes, List<Move> moves)
        {
            if (!promotes)
            {
                moves.Add(new Move(from, to, flags));
                return;
            }

            foreach (PieceType promotion in s_Promotions)
                moves.Add(new Move(from, to, flags | MoveFlags.Promotion, promotion));
        }

        private static void AddSteps(Board board, int from, PieceColor us, (int, int)[] steps, List<Move> moves, bool capturesOnly)
        {
            int file = Square.FileOf(from);
            int rank = Square.RankOf(from);
            foreach ((int df, int dr) in steps)
            {
                if (!Square.OnBoard(file + df, rank + dr))
                    continue;

                int to = Square.At(file + df, rank + dr);
                int target = board.Squares[to];
                if (target == Piece.None)
                {
                    if (!capturesOnly)
                        moves.Add(new Move(from, to));
                }
                else if (Piece.ColorOf(target) != us)
                {
                    moves.Add(new Move(from, to, MoveFlags.Capture));
                }
            }
        }

        private static void AddSlides(Board board, int from, PieceColor us, (int, int)[] directions, List<Move> moves, bool capturesOnly)
        {
            int file = Square.FileOf(from);
            int rank = Square.RankOf(from);
            foreach ((int df, int dr) in directions)
            {
                int f = file + df, r = rank + dr;
                while (Square.OnBoard(f, r))
                {
                    int to = Square.At(f, r);
                    int target = board.Squares[to];
                    if (target == Piece.None)
                    {
                        if (!capturesOnly)
                            moves.Add(new Move(from, to));
                    }
                    else
                    {
                        if (Piece.ColorOf(target) != us)
                            moves.Add(new Move(from, to, MoveFlags.Capture));
                        break;
                    }
                    f += df;
                    r += dr;
                }
            }
        }

        private static void AddCastling(Board board, int from, PieceColor us, List<Move> moves)
        {
            int kingside = us == PieceColor.White ? Board.WhiteKingside : Board.BlackKingside;
            int queenside = us == PieceColor.White ? Board.WhiteQueenside : Board.BlackQueenside;
            if ((board.CastlingRights & (kingside | queenside)) == 0)
                return;

            PieceColor them = Piece.Opponent(us);
            if (board.IsSquareAttacked(from, them))
                return;

            // Rights imply king and rook are still on their home squares.
            if ((board.CastlingRights & kingside) != 0
                && board.Squares[from + 1] == Piece.None && board.Squares[from + 2] == Piece.None
                && !board.IsSquareAttacked(from + 1, them) && !board.IsSquareAttacked(from + 2, them))
            {
                moves.Add(new Move(from, from + 2, MoveFlags.Castle));
            }

            if ((board.CastlingRights & queenside) != 0
                && board.Squares[from - 1] == Piece.None && board.Squares[from - 2] == Piece.None && board.Squares[from - 3] == Piece.None
                && !board.IsSquareAttacked(from - 1, them) && !board.IsSquareAttacked(from - 2, them))
            {
                moves.Add(new Move(from, from - 2, MoveFlags.Castle));
            }
        }
    }
}
