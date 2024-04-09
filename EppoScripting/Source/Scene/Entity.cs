using System;

namespace Eppo
{
	public class Entity
	{
		public Entity()
		{
			Print();
			PrintInt(1);
			PrintInts(1, 2);
			PrintCustom("Niels");
		}

		public void Print()
		{
			Console.WriteLine("C#: Hello!");
		}

		public void PrintInt(int value)
		{
			Console.WriteLine($"C#: Hello {value}!");
		}

		public void PrintInts(int value1, int value2)
		{
			Console.WriteLine($"C#: Hello {value1} & {value2}!");
		}

		public void PrintCustom(string value)
		{
			Console.WriteLine($"C#: Hello {value}!");
		}
	}
}
