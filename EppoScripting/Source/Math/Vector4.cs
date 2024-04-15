namespace Eppo
{
	public struct Vector4
	{
		public float X, Y, Z, W;

		public Vector4(float scalar)
		{
			X = scalar;
			Y = scalar;
			Z = scalar;
			W = scalar;
		}

		public Vector4(float x, float y, float z, float w)
		{
			X = x;
			Y = y;
			Z = z;
			W = w;
		}

		public Vector4(Vector3 xyz, float w)
		{
			X = xyz.X;
			Y = xyz.Y;
			Z = xyz.Z;
			W = w;
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

		public Vector3 XYZ
		{
			get => new Vector3(X, Y, Z);
			set
			{
				X = value.X;
				Y = value.Y;
				Z = value.Z;
			}
		}

		public static Vector4 Zero => new Vector4(0.0f);

		public static Vector4 operator +(Vector4 lhs, Vector4 rhs) => new Vector4(lhs.X + rhs.X, lhs.Y + rhs.Y, lhs.Z + rhs.Z, lhs.W + rhs.W);
		public static Vector4 operator +(Vector4 lhs, float scalar) => new Vector4(lhs.X + scalar, lhs.Y + scalar, lhs.Z + scalar, lhs.W + scalar);
		public static Vector4 operator -(Vector4 lhs, Vector4 rhs) => new Vector4(lhs.X - rhs.X, lhs.Y - rhs.Y, lhs.Z - rhs.Z, lhs.W - rhs.W);
		public static Vector4 operator -(Vector4 lhs, float scalar) => new Vector4(lhs.X - scalar, lhs.Y - scalar, lhs.Z - scalar, lhs.W - scalar);
		public static Vector4 operator *(Vector4 lhs, Vector4 rhs) => new Vector4(lhs.X * rhs.X, lhs.Y * rhs.Y, lhs.Z * rhs.Z, lhs.W * rhs.W);
		public static Vector4 operator *(Vector4 lhs, float scalar) => new Vector4(lhs.X * scalar, lhs.Y * scalar, lhs.Z * scalar, lhs.W * scalar);
		public static Vector4 operator /(Vector4 lhs, Vector4 rhs) => new Vector4(lhs.X / rhs.X, lhs.Y / rhs.Y, lhs.Z / rhs.Z, lhs.W / rhs.W);
		public static Vector4 operator /(Vector4 lhs, float scalar) => new Vector4(lhs.X / scalar, lhs.Y / scalar, lhs.Z / scalar, lhs.W / scalar);
	}
}
