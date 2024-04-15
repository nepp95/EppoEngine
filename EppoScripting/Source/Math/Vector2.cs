namespace Eppo
{
	public struct Vector2
	{
		public float X, Y;

		public Vector2(float scalar)
		{
			X = scalar;
			Y = scalar;
		}

		public Vector2(float x, float y)
		{
			X = x;
			Y = y;
		}

		public override string ToString()
		{
			return $"(X: {X}, Y: {Y})";
		}

		public static Vector2 Zero => new Vector2(0.0f);

		public static Vector2 operator +(Vector2 lhs, Vector2 rhs) => new Vector2(lhs.X + rhs.X, lhs.Y + rhs.Y);
		public static Vector2 operator +(Vector2 lhs, float scalar) => new Vector2(lhs.X + scalar, lhs.Y + scalar);
		public static Vector2 operator -(Vector2 lhs, Vector2 rhs) => new Vector2(lhs.X - rhs.X, lhs.Y - rhs.Y);
		public static Vector2 operator -(Vector2 lhs, float scalar) => new Vector2(lhs.X - scalar, lhs.Y - scalar);
		public static Vector2 operator *(Vector2 lhs, Vector2 rhs) => new Vector2(lhs.X * rhs.X, lhs.Y * rhs.Y);
		public static Vector2 operator *(Vector2 lhs, float scalar) => new Vector2(lhs.X * scalar, lhs.Y * scalar);
		public static Vector2 operator /(Vector2 lhs, Vector2 rhs) => new Vector2(lhs.X / rhs.X, lhs.Y / rhs.Y);
		public static Vector2 operator /(Vector2 lhs, float scalar) => new Vector2(lhs.X / scalar, lhs.Y / scalar);
	}
}
