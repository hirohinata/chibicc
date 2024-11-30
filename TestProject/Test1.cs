using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Diagnostics;

namespace TestProject
{
    [TestClass]
    public sealed class Test1
    {
        private static void CallCompiler(string arguments, out string? asm)
        {
            ProcessStartInfo psInfo = new()
            {
                FileName = "../../../../x64/Debug/chibicc.exe",
                Arguments = $"\"{arguments}\"",
                CreateNoWindow = true,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            var p = Process.Start(psInfo);
            p?.WaitForExit();
            asm = p?.StandardOutput.ReadToEnd();

            var error = p?.StandardError.ReadToEnd();
            Assert.IsTrue(string.IsNullOrEmpty(error), error);
        }

        private static void CallGcc(string? asm, out string exeFileName)
        {
            var tempPath = $"{Path.GetTempPath()}chibicc\\{DateTime.Now.ToString("yyMMdd_HHmmss.fffffff")}";
            Directory.CreateDirectory(tempPath);

            var asmFileName = Path.Combine(tempPath, "test.s");
            exeFileName = Path.Combine(tempPath, "test.exe");
            File.WriteAllText(asmFileName, asm);

            ProcessStartInfo psInfo = new()
            {
                FileName = "cmd",
                Arguments = $"/c \"gcc {asmFileName} -o {exeFileName}\"",
                WorkingDirectory = tempPath,
                CreateNoWindow = true,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            var p = Process.Start(psInfo);
            p?.WaitForExit();

            var error = p?.StandardError.ReadToEnd();
            Assert.IsTrue(string.IsNullOrEmpty(error), error);
        }

        private static void CallExe(string exeFileName, out int exitCode)
        {
            ProcessStartInfo psInfo = new()
            {
                FileName = exeFileName,
                CreateNoWindow = true,
                UseShellExecute = false
            };

            var p = Process.Start(psInfo);
            p?.WaitForExit();
            exitCode = p?.ExitCode ?? -1;
        }

        private static int Compile(string args)
        {
            CallCompiler(args, out var asm);
            CallGcc(asm, out var exeFileName);
            CallExe(exeFileName, out var exitCode);
            return exitCode;
        }

        private static string CompileError(string args)
        {
            ProcessStartInfo psInfo = new()
            {
                FileName = "../../../../x64/Debug/chibicc.exe",
                Arguments = $"\"{args}\"",
                CreateNoWindow = true,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            var p = Process.Start(psInfo);
            p?.WaitForExit();
            return p?.StandardError.ReadToEnd() ?? string.Empty;
        }

        [TestMethod]
        public void TestMethod1()
        {
            Assert.AreEqual(0, Compile("0;"));
            Assert.AreEqual(42, Compile("42;"));
        }

        [TestMethod]
        public void TestMethod2()
        {
            Assert.AreEqual(21, Compile("5+20-4;"));
        }

        [TestMethod]
        public void TestMethod3()
        {
            Assert.AreEqual(41, Compile(" 12 + 34 - 5; "));
        }

        [TestMethod]
        public void TestMethod4()
        {
            Assert.AreEqual("1+3++\r\n     ^ 数ではありません\r\n", CompileError("1+3++"));
            Assert.AreEqual("1+3 2\r\n    ^ ';'ではありません\r\n", CompileError("1+3 2"));
            Assert.AreEqual("1 + @\r\n    ^ トークナイズできません\r\n", CompileError("1 + @"));
        }

        [TestMethod]
        public void TestMethod5()
        {
            Assert.AreEqual(7, Compile("1 + 2 * 3;"));
            Assert.AreEqual(9, Compile("(1 + 2) * 3;"));
            Assert.AreEqual(14, Compile("1 * 2 + 3 * 4;"));
            Assert.AreEqual(1, Compile("(1 + 2) / 3;"));

            Assert.AreEqual(47, Compile("5+6*7;"));
            Assert.AreEqual(15, Compile("5*(9-6);"));
            Assert.AreEqual(4, Compile("(3+5)/2;"));
        }

        [TestMethod]
        public void TestMethod6()
        {
            Assert.AreEqual("1+(3+2\r\n      ^ ')'ではありません\r\n", CompileError("1+(3+2"));
        }

        [TestMethod]
        public void TestMethod7()
        {
            Assert.AreEqual(10, Compile("-10+20;"));
            Assert.AreEqual(17, Compile("+-+12 - -34 - - - 5;"));
        }

        [TestMethod]
        public void TestMethod8()
        {
            Assert.IsTrue(Compile("10 == 4 + 2 * 3;") != 0);
            Assert.IsFalse(Compile("10 == (4 + 2) * 3;") != 0);

            Assert.IsTrue(Compile("10 == 10;") != 0);
            Assert.IsFalse(Compile("10 != 10;") != 0);
            Assert.IsFalse(Compile("10 <  10;") != 0);
            Assert.IsTrue(Compile("10 <= 10;") != 0);
            Assert.IsFalse(Compile("10 >  10;") != 0);
            Assert.IsTrue(Compile("10 >= 10;") != 0);

            Assert.IsFalse(Compile("10 == 11;") != 0);
            Assert.IsTrue(Compile("10 != 11;") != 0);
            Assert.IsTrue(Compile("10 <  11;") != 0);
            Assert.IsTrue(Compile("10 <= 11;") != 0);
            Assert.IsFalse(Compile("10 >  11;") != 0);
            Assert.IsFalse(Compile("10 >= 11;") != 0);

            Assert.IsFalse(Compile("10 == 9;") != 0);
            Assert.IsTrue(Compile("10 != 9;") != 0);
            Assert.IsFalse(Compile("10 <  9;") != 0);
            Assert.IsFalse(Compile("10 <= 9;") != 0);
            Assert.IsTrue(Compile("10 >  9;") != 0);
            Assert.IsTrue(Compile("10 >= 9;") != 0);
        }

        [TestMethod]
        public void TestMethod9()
        {
            Assert.AreEqual(3, Compile("a = 3;"));
            Assert.AreEqual(22, Compile("b = 5 * 6 - 8;"));
            Assert.AreEqual(14, Compile("a = 3; b = 5 * 6 - 8; a + b / 2;"));

            Assert.AreEqual(1, Compile("z = 65535; (z - 1) / 2 - 32766;"));
        }

        [TestMethod]
        public void TestMethod10()
        {
            Assert.AreEqual(6, Compile("foo = 1; bar = 2 + 3; foo + bar;"));
            Assert.AreEqual(6, Compile("a = 1; aa = 2; aaa = 3; a + aa + aaa;"));
        }

        [TestMethod]
        public void TestMethod11()
        {
            Assert.AreEqual(14, Compile("a = 3; b = 5 * 6 - 8; return a + b / 2;"));
            Assert.AreEqual(5, Compile("return 5; return 8;"));
            Assert.AreEqual(11, Compile("returnx = 5; ret = 6; return returnx + ret;"));
        }

        [TestMethod]
        public void TestMethod12()
        {
            Assert.AreEqual(5, Compile("a = 3; if (a == 3) a = 5; return a;"));
            Assert.AreEqual(4, Compile("a = 4; if (a == 3) a = 5; return a;"));
            Assert.AreEqual(5, Compile("a = 3; if (a == 3) a = 5; else a= 6; return a;"));
            Assert.AreEqual(6, Compile("a = 4; if (a == 3) a = 5; else a= 6; return a;"));
        }

        [TestMethod]
        public void TestMethod13()
        {
            Assert.AreEqual(10, Compile("a = 0; while (a < 10) a = a + 1; return a;"));
            Assert.AreEqual(15, Compile("a = 15; while (a < 10) a = a + 1; return a;"));
        }

        [TestMethod]
        public void TestMethod14()
        {
            Assert.AreEqual(10, Compile("a = 0; for (i = 0; i < 5; i = i + 1) a = a + i; return a;"));
            Assert.AreEqual(5, Compile("a = 0; for (i = 1; a < 5;) a = a + i; return a;"));
            Assert.AreEqual(10, Compile("a = 0; for (; a < 10;) a = a + 1; return a;"));
            //TODO:breakを実装していないため、条件式を省略したら無限ループになってしまいテストができない
        }
    }
}
