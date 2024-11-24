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
            var tempPath = $"{Path.GetTempPath()}chibicc\\{Guid.NewGuid().ToString("N").Substring(0, 8)}";
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
                RedirectStandardOutput = true
            };

            Process.Start(psInfo)?.WaitForExit();
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
            Assert.AreEqual(0, Compile("0"));
            Assert.AreEqual(42, Compile("42"));
        }

        [TestMethod]
        public void TestMethod2()
        {
            Assert.AreEqual(21, Compile("5+20-4"));
        }

        [TestMethod]
        public void TestMethod3()
        {
            Assert.AreEqual(41, Compile(" 12 + 34 - 5 "));
        }

        [TestMethod]
        public void TestMethod4()
        {
            Assert.AreEqual("1+3++\r\n    ^ 数ではありません\r\n", CompileError("1+3++"));
            Assert.AreEqual("1+3 2\r\n    ^ '-'ではありません\r\n", CompileError("1+3 2"));
            Assert.AreEqual("1 + foo + 5\r\n    ^ トークナイズできません\r\n", CompileError("1 + foo + 5"));
        }
    }
}
