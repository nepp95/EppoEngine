namespace Eppo
{
	public struct Vector3
	{
		public float X, Y, Z;

		public Vector3(float scalar)
		{
			X = scalar;
			Y = scalar;
			Z = scalar;
		}

		public Vector3(float x, float y, float z)
		{
			X = x;
			Y = y;
			Z = z;
		}

		public Vector3(Vector2 xy, float z)
		{
			X = xy.X;
			Y = xy.Y;
			Z = z;
		}

		public override string ToString()
		{
			return $"(X: {X}, Y: {Y}, Z: {Z})";
		}

		public Vector2 XY
		{
			get => new Vector2(X, Y);
			set
			{
				X = value.X;
				Y = value.Y;
			}
		}

		public static Vector3 Zero => new Vector3(0.0f);

		public static Vector3 operator +(Vector3 lhs, Vector3 rhs) => new Vector3(lhs.X + rhs.X, lhs.Y + rhs.Y, lhs.Z + rhs.Z);
		public static Vector3 operator +(Vector3 lhs, float scalar) => new Vector3(lhs.X + scalar, lhs.Y + scalar, lhs.Z + scalar);
		public static Vector3 operator -(Vector3 lhs, Vector3 rhs) => new Vector3(lhs.X - rhs.X, lhs.Y - rhs.Y, lhs.Z - rhs.Z);
		public static Vector3 operator -(Vector3 lhs, float scalar) => new Vector3(lhs.X - scalar, lhs.Y - scalar, lhs.Z - scalar);
		public static Vector3 operator *(Vector3 lhs, Vector3 rhs) => new Vector3(lhs.X * rhs.X, lhs.Y * rhs.Y, lhs.Z * rhs.Z);
		public static Vector3 operator *(Vector3 lhs, float scalar) => new Vector3(lhs.X * scalar, lhs.Y * scalar, lhs.Z * scalar);
		public static Vector3 operator /(Vector3 lhs, Vector3 rhs) => new Vector3(lhs.X / rhs.X, lhs.Y / rhs.Y, lhs.Z / rhs.Z);
		public static Vector3 operator /(Vector3 lhs, float scalar) => new Vector3(lhs.X / scalar, lhs.Y / scalar, lhs.Z / scalar);
	}
}
