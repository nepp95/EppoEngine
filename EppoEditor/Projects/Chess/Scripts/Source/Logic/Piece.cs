namespace Chess.Logic
{
    public enum PieceColor
    {
        White = 0,
        Black = 1,
    }

    public enum PieceType
    {
        None = 0,
        Pawn = 1,
        Knight = 2,
        Bishop = 3,
        Rook = 4,
        Queen = 5,
        King = 6,
    }

    // A piece is packed into one int: the type in the low three bits, plus
    // BlackFlag for Black. Zero is an empty square.
    public static class Piece
    {
        public const int None = 0;
        public const int BlackFlag = 8;

        public static int Make(PieceColor color, PieceType type)
            => (int)type | (color == PieceColor.Black ? BlackFlag : 0);

        public static PieceType TypeOf(int piece) => (PieceType)(piece & 7);

        public static PieceColor ColorOf(int piece)
            => (piece & BlackFlag) != 0 ? PieceColor.Black : PieceColor.White;

        public static bool IsColor(int piece, PieceColor color)
            => piece != None && ColorOf(piece) == color;

        public static PieceColor Opponent(PieceColor color)
            => color == PieceColor.White ? PieceColor.Black : PieceColor.White;
    }

    // Squares are 0..63 with a1 = 0, h1 = 7, a8 = 56, h8 = 63.
    public static class Square
    {
        public const int None = -1;

        public static int FileOf(int square) => square & 7;
        public static int RankOf(int square) => square >> 3;
        public static int At(int file, int rank) => rank * 8 + file;
        public static bool OnBoard(int file, int rank) => file >= 0 && file < 8 && rank >= 0 && rank < 8;

        public static string Name(int square)
            => square == None ? "-" : $"{(char)('a' + FileOf(square))}{RankOf(square) + 1}";

        public static int Parse(string name)
            => name == "-" ? None : At(name[0] - 'a', name[1] - '1');
    }
}
