using System.Text;

namespace Chess.Logic
{
    public sealed class Board
    {
        public const string StartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        public const int WhiteKingside = 1;
        public const int WhiteQueenside = 2;
        public const int BlackKingside = 4;
        public const int BlackQueenside = 8;

        public readonly int[] Squares = new int[64];
        public PieceColor SideToMove { get; private set; }
        public int CastlingRights { get; private set; }
        public int EnPassantSquare { get; private set; } = Square.None;
        public int HalfmoveClock { get; private set; }
        public int FullmoveNumber { get; private set; } = 1;

        private readonly int[] m_KingSquares = new int[2];

        // Rights lost when a move touches a square (king or rook moved/captured).
        private static readonly int[] s_CastlingMask = BuildCastlingMask();

        private static readonly int[] s_KnightOffsets = { 1, 2, 2, 1, -1, -2, -2, -1, 1, -2, 2, -1, -1, 2, -2, 1 };
        private static readonly int[] s_KingOffsets = { 1, 0, 1, 1, 0, 1, -1, 1, -1, 0, -1, -1, 0, -1, 1, -1 };

        public static Board FromFen(string fen)
        {
            var board = new Board();
            string[] parts = fen.Split(' ', StringSplitOptions.RemoveEmptyEntries);

            int rank = 7, file = 0;
            foreach (char c in parts[0])
            {
                if (c == '/')
                {
                    rank--;
                    file = 0;
                }
                else if (char.IsDigit(c))
                {
                    file += c - '0';
                }
                else
                {
                    PieceColor color = char.IsUpper(c) ? PieceColor.White : PieceColor.Black;
                    PieceType type = char.ToLowerInvariant(c) switch
                    {
                        'p' => PieceType.Pawn,
                        'n' => PieceType.Knight,
                        'b' => PieceType.Bishop,
                        'r' => PieceType.Rook,
                        'q' => PieceType.Queen,
                        _ => PieceType.King,
                    };
                    int square = Square.At(file, rank);
                    board.Squares[square] = Piece.Make(color, type);
                    if (type == PieceType.King)
                        board.m_KingSquares[(int)color] = square;
                    file++;
                }
            }

            board.SideToMove = parts.Length > 1 && parts[1] == "b" ? PieceColor.Black : PieceColor.White;

            if (parts.Length > 2)
            {
                foreach (char c in parts[2])
                {
                    board.CastlingRights |= c switch
                    {
                        'K' => WhiteKingside,
                        'Q' => WhiteQueenside,
                        'k' => BlackKingside,
                        'q' => BlackQueenside,
                        _ => 0,
                    };
                }
            }

            board.EnPassantSquare = parts.Length > 3 ? Square.Parse(parts[3]) : Square.None;
            board.HalfmoveClock = parts.Length > 4 ? int.Parse(parts[4]) : 0;
            board.FullmoveNumber = parts.Length > 5 ? int.Parse(parts[5]) : 1;
            return board;
        }

        public Board Clone()
        {
            var copy = new Board
            {
                SideToMove = SideToMove,
                CastlingRights = CastlingRights,
                EnPassantSquare = EnPassantSquare,
                HalfmoveClock = HalfmoveClock,
                FullmoveNumber = FullmoveNumber,
            };
            Array.Copy(Squares, copy.Squares, 64);
            Array.Copy(m_KingSquares, copy.m_KingSquares, 2);
            return copy;
        }

        public int KingSquare(PieceColor color) => m_KingSquares[(int)color];

        public bool InCheck(PieceColor color)
            => IsSquareAttacked(m_KingSquares[(int)color], Piece.Opponent(color));

        public bool IsSquareAttacked(int square, PieceColor by)
        {
            int file = Square.FileOf(square);
            int rank = Square.RankOf(square);

            // Pawns attack diagonally forward, so look one rank "behind" the square.
            int pawnRank = by == PieceColor.White ? rank - 1 : rank + 1;
            int pawn = Piece.Make(by, PieceType.Pawn);
            if (Square.OnBoard(file - 1, pawnRank) && Squares[Square.At(file - 1, pawnRank)] == pawn)
                return true;
            if (Square.OnBoard(file + 1, pawnRank) && Squares[Square.At(file + 1, pawnRank)] == pawn)
                return true;

            if (AttackedByLeaper(file, rank, s_KnightOffsets, Piece.Make(by, PieceType.Knight)))
                return true;
            if (AttackedByLeaper(file, rank, s_KingOffsets, Piece.Make(by, PieceType.King)))
                return true;

            int queen = Piece.Make(by, PieceType.Queen);
            int rook = Piece.Make(by, PieceType.Rook);
            int bishop = Piece.Make(by, PieceType.Bishop);

            return AttackedBySlider(file, rank, 1, 0, rook, queen) || AttackedBySlider(file, rank, -1, 0, rook, queen)
                || AttackedBySlider(file, rank, 0, 1, rook, queen) || AttackedBySlider(file, rank, 0, -1, rook, queen)
                || AttackedBySlider(file, rank, 1, 1, bishop, queen) || AttackedBySlider(file, rank, 1, -1, bishop, queen)
                || AttackedBySlider(file, rank, -1, 1, bishop, queen) || AttackedBySlider(file, rank, -1, -1, bishop, queen);
        }

        private bool AttackedByLeaper(int file, int rank, int[] offsets, int attacker)
        {
            for (int i = 0; i < offsets.Length; i += 2)
            {
                int f = file + offsets[i];
                int r = rank + offsets[i + 1];
                if (Square.OnBoard(f, r) && Squares[Square.At(f, r)] == attacker)
                    return true;
            }
            return false;
        }

        private bool AttackedBySlider(int file, int rank, int df, int dr, int slider, int queen)
        {
            int f = file + df, r = rank + dr;
            while (Square.OnBoard(f, r))
            {
                int piece = Squares[Square.At(f, r)];
                if (piece != Piece.None)
                    return piece == slider || piece == queen;
                f += df;
                r += dr;
            }
            return false;
        }

        public Undo MakeMove(Move move)
        {
            int piece = Squares[move.From];
            PieceColor us = SideToMove;

            int capturedSquare = move.IsEnPassant
                ? move.To + (us == PieceColor.White ? -8 : 8)
                : move.To;
            int captured = Squares[capturedSquare];
            var undo = new Undo(captured, capturedSquare, CastlingRights, EnPassantSquare, HalfmoveClock);

            Squares[capturedSquare] = Piece.None;
            Squares[move.From] = Piece.None;
            Squares[move.To] = move.IsPromotion ? Piece.Make(us, move.Promotion) : piece;

            if (Piece.TypeOf(piece) == PieceType.King)
            {
                m_KingSquares[(int)us] = move.To;

                if (move.IsCastle)
                {
                    // The rook jumps to the square the king passed over.
                    bool kingside = move.To > move.From;
                    int rookFrom = kingside ? move.From + 3 : move.From - 4;
                    int rookTo = kingside ? move.From + 1 : move.From - 1;
                    Squares[rookTo] = Squares[rookFrom];
                    Squares[rookFrom] = Piece.None;
                }
            }

            CastlingRights &= s_CastlingMask[move.From] & s_CastlingMask[move.To];
            EnPassantSquare = (move.Flags & MoveFlags.DoublePush) != 0 ? (move.From + move.To) / 2 : Square.None;
            HalfmoveClock = Piece.TypeOf(piece) == PieceType.Pawn || captured != Piece.None ? 0 : HalfmoveClock + 1;
            if (us == PieceColor.Black)
                FullmoveNumber++;
            SideToMove = Piece.Opponent(us);

            return undo;
        }

        public void UnmakeMove(Move move, Undo undo)
        {
            SideToMove = Piece.Opponent(SideToMove);
            PieceColor us = SideToMove;
            if (us == PieceColor.Black)
                FullmoveNumber--;

            int piece = Squares[move.To];
            if (move.IsPromotion)
                piece = Piece.Make(us, PieceType.Pawn);

            Squares[move.From] = piece;
            Squares[move.To] = Piece.None;
            Squares[undo.CapturedSquare] = undo.Captured;

            if (Piece.TypeOf(piece) == PieceType.King)
            {
                m_KingSquares[(int)us] = move.From;

                if (move.IsCastle)
                {
                    bool kingside = move.To > move.From;
                    int rookFrom = kingside ? move.From + 3 : move.From - 4;
                    int rookTo = kingside ? move.From + 1 : move.From - 1;
                    Squares[rookFrom] = Squares[rookTo];
                    Squares[rookTo] = Piece.None;
                }
            }

            CastlingRights = undo.CastlingRights;
            EnPassantSquare = undo.EnPassantSquare;
            HalfmoveClock = undo.HalfmoveClock;
        }

        // Identifies a position for repetition checks: placement, side, rights and
        // en passant target (the move counters don't matter).
        public string PositionKey()
        {
            var sb = new StringBuilder(80);
            foreach (int piece in Squares)
                sb.Append((char)('A' + piece));
            sb.Append((int)SideToMove).Append(CastlingRights).Append(':').Append(EnPassantSquare);
            return sb.ToString();
        }

        private static int[] BuildCastlingMask()
        {
            var mask = new int[64];
            Array.Fill(mask, 15);
            mask[Square.Parse("e1")] &= ~(WhiteKingside | WhiteQueenside);
            mask[Square.Parse("h1")] &= ~WhiteKingside;
            mask[Square.Parse("a1")] &= ~WhiteQueenside;
            mask[Square.Parse("e8")] &= ~(BlackKingside | BlackQueenside);
            mask[Square.Parse("h8")] &= ~BlackKingside;
            mask[Square.Parse("a8")] &= ~BlackQueenside;
            return mask;
        }
    }
}
