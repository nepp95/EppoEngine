namespace Chess.Logic
{
    [Flags]
    public enum MoveFlags : byte
    {
        None = 0,
        Capture = 1 << 0,
        EnPassant = 1 << 1,
        Castle = 1 << 2,
        DoublePush = 1 << 3,
        Promotion = 1 << 4,
    }

    public readonly struct Move : IEquatable<Move>
    {
        public readonly int From;
        public readonly int To;
        public readonly PieceType Promotion;
        public readonly MoveFlags Flags;

        public Move(int from, int to, MoveFlags flags = MoveFlags.None, PieceType promotion = PieceType.None)
        {
            From = from;
            To = to;
            Flags = flags;
            Promotion = promotion;
        }

        public bool IsCapture => (Flags & MoveFlags.Capture) != 0;
        public bool IsCastle => (Flags & MoveFlags.Castle) != 0;
        public bool IsEnPassant => (Flags & MoveFlags.EnPassant) != 0;
        public bool IsPromotion => (Flags & MoveFlags.Promotion) != 0;

        public bool Equals(Move other)
            => From == other.From && To == other.To && Promotion == other.Promotion && Flags == other.Flags;

        public override bool Equals(object? obj) => obj is Move other && Equals(other);
        public override int GetHashCode() => HashCode.Combine(From, To, Promotion, Flags);

        public override string ToString()
        {
            string text = Square.Name(From) + Square.Name(To);
            if (IsPromotion)
                text += "nbrq"[(int)Promotion - (int)PieceType.Knight];
            return text;
        }
    }

    // Everything MakeMove destroys, so UnmakeMove can restore it exactly.
    public readonly struct Undo
    {
        public readonly int Captured;
        public readonly int CapturedSquare;
        public readonly int CastlingRights;
        public readonly int EnPassantSquare;
        public readonly int HalfmoveClock;

        public Undo(int captured, int capturedSquare, int castlingRights, int enPassantSquare, int halfmoveClock)
        {
            Captured = captured;
            CapturedSquare = capturedSquare;
            CastlingRights = castlingRights;
            EnPassantSquare = enPassantSquare;
            HalfmoveClock = halfmoveClock;
        }
    }
}
