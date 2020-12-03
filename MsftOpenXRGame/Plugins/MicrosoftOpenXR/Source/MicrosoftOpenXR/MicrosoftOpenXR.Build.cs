// Copyright (c) 2020 Microsoft Corporation.
// Licensed under the MIT License.

using System.IO;
using System.Linq;
using System.Runtime.Remoting.Messaging;
using UnrealBuildTool;
using Tools.DotNETCommon;
using System;

public class MicrosoftOpenXR : ModuleRules
{
	public MicrosoftOpenXR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = @"Private\OpenXRCommon.h";

		PrivateIncludePaths.AddRange(
			new string[] {
				// This private include path ensures our newer copy of the openxr headers take precedence over the engine's copy.
				"MicrosoftOpenXR/Private/External"
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"OpenXRHMD",
				"MicrosoftOpenXRRuntimeSettings",
				"HeadMountedDisplay",
				"AugmentedReality",
				"OpenXRAR",
				"RHI",
				"RenderCore",
				"Projects",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd"
				}
			);
		}

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"HeadMountedDisplay"
			}
			);

		PublicIncludePathModuleNames.AddRange(
			new string[]
			{
				"HeadMountedDisplay"
			}
			);

		// WinRT with Nuget support
		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.HoloLens)
		{
			// these parameters mandatory for winrt support
			bEnableExceptions = true;
			bUseUnity = false;
			CppStandard = CppStandardVersion.Cpp17;
			PublicSystemLibraries.AddRange(new string [] { "shlwapi.lib", "runtimeobject.lib" });

			// prepare everything for nuget
			string MyModuleName = GetType().Name;
			string NugetFolder = Path.Combine(PluginDirectory, "Intermediate", "Nuget", MyModuleName);
			Directory.CreateDirectory(NugetFolder);

			string BinariesSubFolder = Path.Combine("Binaries", "ThirdParty", Target.Type.ToString(), Target.Platform.ToString(), Target.Architecture);

			PrivateDefinitions.Add(string.Format("THIRDPARTY_BINARY_SUBFOLDER=\"{0}\"", BinariesSubFolder.Replace(@"\", @"\\")));

			string BinariesFolder = Path.Combine(PluginDirectory, BinariesSubFolder);
			Directory.CreateDirectory(BinariesFolder);

			// download nuget
			string NugetExe = Path.Combine(NugetFolder, "nuget.exe");
			if (!File.Exists(NugetExe))
			{
				using (System.Net.WebClient myWebClient = new System.Net.WebClient())
				{
					// we aren't focusing on a specific nuget version, we can use any of them but the latest one is preferable
					myWebClient.DownloadFile(@"https://dist.nuget.org/win-x86-commandline/latest/nuget.exe", NugetExe);
				}
			}

			// run nuget to update the packages
			{
				var StartInfo = new System.Diagnostics.ProcessStartInfo(NugetExe, string.Format("install \"{0}\" -OutputDirectory \"{1}\"", Path.Combine(ModuleDirectory, "packages.config"), NugetFolder));
				StartInfo.UseShellExecute = false;
				StartInfo.CreateNoWindow = true;
				var ExitCode = Utils.RunLocalProcessAndPrintfOutput(StartInfo);
				if (ExitCode < 0)
				{
					throw new BuildException("Failed to get nuget packages.  See log for details.");
				}
			}

			// get list of the installed packages, that's needed because the code should get particular versions of the installed packages
			string[] InstalledPackages = Utils.RunLocalProcessAndReturnStdOut(NugetExe, string.Format("list -Source \"{0}\"", NugetFolder)).Split(new char[] { '\r', '\n' });

			// get WinRT package 
			string CppWinRTPackage = InstalledPackages.FirstOrDefault(x => x.StartsWith("Microsoft.Windows.CppWinRT"));
			if (!string.IsNullOrEmpty(CppWinRTPackage))
			{
				string CppWinRTName = CppWinRTPackage.Replace(" ", ".");
				string CppWinRTExe = Path.Combine(NugetFolder, CppWinRTName, "bin", "cppwinrt.exe");
				string CppWinRTFolder = Path.Combine(PluginDirectory, "Intermediate", CppWinRTName, MyModuleName);
				Directory.CreateDirectory(CppWinRTFolder);

				// search all downloaded packages for winmd files
				string[] WinMDFiles = Directory.GetFiles(NugetFolder, "*.winmd", SearchOption.AllDirectories);

				// all downloaded winmd file with WinSDK to be processed by cppwinrt.exe
				var WinMDFilesStringbuilder = new System.Text.StringBuilder();
				foreach (var winmd in WinMDFiles)
				{
					WinMDFilesStringbuilder.Append(" -input \"");
					WinMDFilesStringbuilder.Append(winmd);
					WinMDFilesStringbuilder.Append("\"");
				}

				// generate winrt headers and add them into include paths
				var StartInfo = new System.Diagnostics.ProcessStartInfo(CppWinRTExe, string.Format("{0} -input \"{1}\" -output \"{2}\"", WinMDFilesStringbuilder, Target.WindowsPlatform.WindowsSdkVersion, CppWinRTFolder));
				StartInfo.UseShellExecute = false;
				StartInfo.CreateNoWindow = true;
				var ExitCode = Utils.RunLocalProcessAndPrintfOutput(StartInfo);
				if (ExitCode < 0)
				{
					throw new BuildException("Failed to get generate WinRT headers.  See log for details.");
				}

				PrivateIncludePaths.Add(CppWinRTFolder);
			}
			else
			{
				// fall back to default WinSDK headers if no winrt package in our list
				PrivateIncludePaths.Add(Path.Combine(Target.WindowsPlatform.WindowsSdkDir, "Include", Target.WindowsPlatform.WindowsSdkVersion, "cppwinrt"));
			}

			// WinRT lib for some job
			string QRPackage = InstalledPackages.FirstOrDefault(x => x.StartsWith("Microsoft.MixedReality.QR"));
			if (!string.IsNullOrEmpty(QRPackage))
			{
				string QRFolderName = QRPackage.Replace(" ", ".");

				// copying dll and winmd binaries to our local binaries folder
				// !!!!! please make sure that you use the path of file! Unreal can't do it for you !!!!!
				SafeCopy(Path.Combine(NugetFolder, QRFolderName, @"lib\uap10.0.18362\Microsoft.MixedReality.QR.winmd"),
					Path.Combine(BinariesFolder, "Microsoft.MixedReality.QR.winmd"));

				SafeCopy(Path.Combine(NugetFolder, QRFolderName, string.Format(@"runtimes\win10-{0}\native\Microsoft.MixedReality.QR.dll", Target.WindowsPlatform.Architecture.ToString())),
					Path.Combine(BinariesFolder, "Microsoft.MixedReality.QR.dll"));

				// also both both binaries must be in RuntimeDependencies, unless you get failures in Hololens platform
				RuntimeDependencies.Add(Path.Combine(BinariesFolder, "Microsoft.MixedReality.QR.dll"));
				RuntimeDependencies.Add(Path.Combine(BinariesFolder, "Microsoft.MixedReality.QR.winmd"));
			}

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				// Microsoft.VCRTForwarders.140 is needed to run WinRT dlls in Win64 platforms
				string VCRTForwardersPackage = InstalledPackages.FirstOrDefault(x => x.StartsWith("Microsoft.VCRTForwarders.140"));
				if (!string.IsNullOrEmpty(VCRTForwardersPackage))
				{
					string VCRTForwardersName = VCRTForwardersPackage.Replace(" ", ".");
					foreach (var Dll in Directory.EnumerateFiles(Path.Combine(NugetFolder, VCRTForwardersName, "runtimes/win10-x64/native/release"), "*_app.dll"))
					{
						string newDll = Path.Combine(BinariesFolder, Path.GetFileName(Dll));
						SafeCopy(Dll, newDll);
						RuntimeDependencies.Add(newDll);
					}
				}

				string RemotingPackage = InstalledPackages.FirstOrDefault(x => x.StartsWith("Microsoft.Holographic.Remoting.OpenXr"));
				if (!string.IsNullOrEmpty(RemotingPackage))
				{
					string RemotingFolderName = RemotingPackage.Replace(" ", ".");

					SafeCopy(Path.Combine(NugetFolder, RemotingFolderName, @"build\native\bin\x64\Desktop\Microsoft.Holographic.AppRemoting.OpenXr.dll"),
						Path.Combine(BinariesFolder, "Microsoft.Holographic.AppRemoting.OpenXr.dll"));

					SafeCopy(Path.Combine(NugetFolder, RemotingFolderName, @"build\native\bin\x64\Desktop\RemotingXR.json"),
						Path.Combine(BinariesFolder, "RemotingXR.json"));

					PrivateIncludePaths.Add(Path.Combine(NugetFolder, RemotingFolderName, @"build\native\include\openxr"));

					RuntimeDependencies.Add(Path.Combine(BinariesFolder, "Microsoft.Holographic.AppRemoting.OpenXr.dll"));
					RuntimeDependencies.Add(Path.Combine(BinariesFolder, "RemotingXR.json"));
				}
			}
		}
	}

	private void SafeCopy(string source, string destination)
	{
		if(!File.Exists(source))
		{
			Log.TraceError("Class {0} can't find {1} file for copying", this.GetType().Name, source);
			return;
		}

		try
		{
			File.Copy(source, destination, true);
		}
		catch(IOException ex)
		{
			Log.TraceWarning("Failed to copy {0} to {1} with exception: {2}", source, destination, ex.Message);
			if (!File.Exists(destination))
			{
				Log.TraceError("Destination file {0} does not exist", destination);
				return;
			}

			Log.TraceWarning("Destination file {0} already existed and is probably in use.  The old file will be used for the runtime dependency.  This may happen when packaging a Win64 exe from the editor.", destination);
		}
	}
}
