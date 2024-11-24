using System.Diagnostics;

namespace TestProject
{
    [TestClass]
    public sealed class Test1
    {
        private static void CallCompiler(string artuments, out string? asm)
        {
            ProcessStartInfo psInfo = new()
            {
                FileName = "../../../../x64/Debug/chibicc.exe",
                Arguments = artuments,
                CreateNoWindow = true,
                UseShellExecute = false,
                RedirectStandardOutput = true
            };

            asm = Process.Start(psInfo)?.StandardOutput.ReadToEnd();
        }

        private static void CallGcc(string? asm, out string exeFileName)
        {
            var tempPath = $"{Path.GetTempPath()}chibicc";
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

        [TestMethod]
        public void TestMethod1()
        {
            Assert.AreEqual(0, Compile("0"));
            Assert.AreEqual(42, Compile("42"));
        }
    }
}
