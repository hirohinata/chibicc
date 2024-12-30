using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Diagnostics;

namespace TestProject
{
    [TestClass]
    public sealed class Test1
    {
        private static void CallCompiler(string arguments, out string asm)
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
            asm = p?.StandardOutput.ReadToEnd() ?? string.Empty;

            var error = p?.StandardError.ReadToEnd();
            Assert.IsTrue(string.IsNullOrEmpty(error), error);
        }

        private static void CallGcc(string asm, string? otherCode, out string exeFileName)
        {
            var tempPath = $"{Path.GetTempPath()}chibicc\\{DateTime.Now.ToString("yyMMdd_HHmmss.fffffff")}";
            Directory.CreateDirectory(tempPath);

            string outFileName;
            {
                var asmFileName = Path.Combine(tempPath, "test.s");
                File.WriteAllText(asmFileName, asm);

                outFileName = Path.Combine(tempPath, "test.o");
                Core(tempPath, $"-c {asmFileName}", outFileName);
            }

            string otherOutFileName = string.Empty;
            if (otherCode != null)
            {
                var otherFileName = Path.Combine(tempPath, "other.c");
                File.WriteAllText(otherFileName, otherCode);

                otherOutFileName = Path.Combine(tempPath, "other.o");
                Core(tempPath, $"-c {otherFileName}", otherOutFileName);
            }

            exeFileName = Path.Combine(tempPath, "test.exe");
            Core(tempPath, $"{outFileName} {otherOutFileName}", exeFileName);

            static void Core(string directory, string input, string output)
            {
                ProcessStartInfo psInfo = new()
                {
                    FileName = "cmd",
                    Arguments = $"/c \"gcc {input} -o {output}\"",
                    WorkingDirectory = directory,
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

        private static int Compile(string args, string? otherCode = null)
        {
            CallCompiler(args, out var asm);
            CallGcc(asm, otherCode, out var exeFileName);
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
            Assert.AreEqual(0, Compile("int main() { 0; }"));
            Assert.AreEqual(42, Compile("int main() { 42; }"));
        }

        [TestMethod]
        public void TestMethod2()
        {
            Assert.AreEqual(21, Compile("int main() { 5+20-4; }"));
        }

        [TestMethod]
        public void TestMethod3()
        {
            Assert.AreEqual(41, Compile("int main() {  12 + 34 - 5; }"));
        }

        [TestMethod]
        public void TestMethod4()
        {
            Assert.AreEqual("int main() { 1+3++ }\r\n                   ^ 数ではありません\r\n", CompileError("int main() { 1+3++ }"));
            Assert.AreEqual("int main() { 1+3 2 }\r\n                 ^ ';'ではありません\r\n", CompileError("int main() { 1+3 2 }"));
            Assert.AreEqual("int main() { 1 + @ }\r\n                 ^ トークナイズできません\r\n", CompileError("int main() { 1 + @ }"));
        }

        [TestMethod]
        public void TestMethod5()
        {
            Assert.AreEqual(7, Compile("int main() { 1 + 2 * 3; }"));
            Assert.AreEqual(9, Compile("int main() { (1 + 2) * 3; }"));
            Assert.AreEqual(14, Compile("int main() { 1 * 2 + 3 * 4; }"));
            Assert.AreEqual(1, Compile("int main() { (1 + 2) / 3; }"));

            Assert.AreEqual(47, Compile("int main() { 5+6*7; }"));
            Assert.AreEqual(15, Compile("int main() { 5*(9-6); }"));
            Assert.AreEqual(4, Compile("int main() { (3+5)/2; }"));
        }

        [TestMethod]
        public void TestMethod6()
        {
            Assert.AreEqual("int main() { 1+(3+2 }\r\n                    ^ ')'ではありません\r\n", CompileError("int main() { 1+(3+2 }"));
        }

        [TestMethod]
        public void TestMethod7()
        {
            Assert.AreEqual(10, Compile("int main() { -10+20; }"));
            Assert.AreEqual(17, Compile("int main() { +-+12 - -34 - - - 5; }"));
        }

        [TestMethod]
        public void TestMethod8()
        {
            Assert.IsTrue(Compile("int main() { 10 == 4 + 2 * 3; }") != 0);
            Assert.IsFalse(Compile("int main() { 10 == (4 + 2) * 3; }") != 0);

            Assert.IsTrue(Compile("int main() { 10 == 10; }") != 0);
            Assert.IsFalse(Compile("int main() { 10 != 10; }") != 0);
            Assert.IsFalse(Compile("int main() { 10 <  10; }") != 0);
            Assert.IsTrue(Compile("int main() { 10 <= 10; }") != 0);
            Assert.IsFalse(Compile("int main() { 10 >  10; }") != 0);
            Assert.IsTrue(Compile("int main() { 10 >= 10; }") != 0);

            Assert.IsFalse(Compile("int main() { 10 == 11; }") != 0);
            Assert.IsTrue(Compile("int main() { 10 != 11; }") != 0);
            Assert.IsTrue(Compile("int main() { 10 <  11; }") != 0);
            Assert.IsTrue(Compile("int main() { 10 <= 11; }") != 0);
            Assert.IsFalse(Compile("int main() { 10 >  11; }") != 0);
            Assert.IsFalse(Compile("int main() { 10 >= 11; }") != 0);

            Assert.IsFalse(Compile("int main() { 10 == 9; }") != 0);
            Assert.IsTrue(Compile("int main() { 10 != 9; }") != 0);
            Assert.IsFalse(Compile("int main() { 10 <  9; }") != 0);
            Assert.IsFalse(Compile("int main() { 10 <= 9; }") != 0);
            Assert.IsTrue(Compile("int main() { 10 >  9; }") != 0);
            Assert.IsTrue(Compile("int main() { 10 >= 9; }") != 0);
        }

        [TestMethod]
        public void TestMethod9()
        {
            Assert.AreEqual(3, Compile("int main() { int a; a = 3; }"));
            Assert.AreEqual(22, Compile("int main() { int b; b = 5 * 6 - 8; }"));
            Assert.AreEqual(14, Compile("int main() { int a; int b; a = 3; b = 5 * 6 - 8; a + b / 2; }"));

            Assert.AreEqual(1, Compile("int main() { int z; z = 65535; (z - 1) / 2 - 32766; }"));
        }

        [TestMethod]
        public void TestMethod10()
        {
            Assert.AreEqual(6, Compile("int main() { int foo; int bar; foo = 1; bar = 2 + 3; foo + bar; }"));
            Assert.AreEqual(6, Compile("int main() { int a; int aa; int aaa; a = 1; aa = 2; aaa = 3; a + aa + aaa; }"));
        }

        [TestMethod]
        public void TestMethod11()
        {
            Assert.AreEqual(14, Compile("int main() { int a; int b; a = 3; b = 5 * 6 - 8; return a + b / 2; }"));
            Assert.AreEqual(5, Compile("int main() { return 5; return 8; }"));
            Assert.AreEqual(11, Compile("int main() { int returnx; int ret; returnx = 5; ret = 6; return returnx + ret; }"));
        }

        [TestMethod]
        public void TestMethod12()
        {
            Assert.AreEqual(5, Compile("int main() { int a; a = 3; if (a == 3) a = 5; return a; }"));
            Assert.AreEqual(4, Compile("int main() { int a; a = 4; if (a == 3) a = 5; return a; }"));
            Assert.AreEqual(5, Compile("int main() { int a; a = 3; if (a == 3) a = 5; else a= 6; return a; }"));
            Assert.AreEqual(6, Compile("int main() { int a; a = 4; if (a == 3) a = 5; else a= 6; return a; }"));
        }

        [TestMethod]
        public void TestMethod13()
        {
            Assert.AreEqual(10, Compile("int main() { int a; a = 0; while (a < 10) a = a + 1; return a; }"));
            Assert.AreEqual(15, Compile("int main() { int a; a = 15; while (a < 10) a = a + 1; return a; }"));
        }

        [TestMethod]
        public void TestMethod14()
        {
            Assert.AreEqual(10, Compile("int main() { int a; int i; a = 0; for (i = 0; i < 5; i = i + 1) a = a + i; return a; }"));
            Assert.AreEqual(5, Compile("int main() { int a; int i; a = 0; for (i = 1; a < 5;) a = a + i; return a; }"));
            Assert.AreEqual(10, Compile("int main() { int a; a = 0; for (; a < 10;) a = a + 1; return a; }"));
            //TODO:breakを実装していないため、条件式を省略したら無限ループになってしまいテストができない
        }

        [TestMethod]
        public void TestMethod15()
        {
            Assert.AreEqual(4, Compile("int main() { int a; int b; a = 1; b = 2; if (a < b) { a = 4; b = 5; } else { a = 6; b = 7; } return a; }"));
            Assert.AreEqual(6, Compile("int main() { int a; int b; a = 3; b = 2; if (a < b) { a = 4; b = 5; } else { a = 6; b = 7; } return a; }"));
            Assert.AreEqual(8, Compile("int main() { int a; a = 8; {} {{}} return a; }"));
        }

        [TestMethod]
        public void TestMethod16()
        {
            Assert.AreEqual(42, Compile("int main() { return foo(); }", "int foo() { return 42; }"));
            Assert.AreEqual(52, Compile("int main() { return foo(10); }", "int foo(int a) { return a + 42; }"));
            Assert.AreEqual(13, Compile("int main() { return foo(2, 3); }", "int foo(int a, int b) { return (a * 5) + b; }"));
            Assert.AreEqual(32, Compile("int main() { return foo(1, 2, 3, 4); }", "int foo(int a, int b, int c, int d) { return (a * 5) + b - c + (d * 7); }"));
        }

        [TestMethod]
        public void TestMethod17()
        {
            Assert.AreEqual(42, Compile("int main() { return foo(); } int foo() { return 42; }"));
            Assert.AreEqual(52, Compile("int main() { return foo() + bar(); } int foo() { return 42; } int bar() { return 10; }"));
            Assert.AreEqual(52, Compile("int main() { return foo(10); } int foo(int a) { return a + 42; }"));
            Assert.AreEqual(13, Compile("int main() { return foo(2, 3); } int foo(int a, int b) { return (a * 5) + b; }"));
            Assert.AreEqual(32, Compile("int main() { return foo(1, 2, 3, 4); } int foo(int a, int b, int c, int d) { return (a * 5) + b - c + (d * 7); }"));
        }

        [TestMethod]
        public void TestMethod18()
        {
            Assert.AreEqual(3, Compile("int main() { int x; int* y; x = 3; y = &x; return *y; }"));
        }
    }
}
