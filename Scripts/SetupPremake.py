import os
import stat
import platform
from pathlib import Path

import Utils

class PremakeConfiguration:
    premakeVersion = "5.0.0-beta3"
    premakeLicenseUrl = "https://raw.githubusercontent.com/premake/premake-core/master/LICENSE.txt"
    premakeDirectory = "./Vendor/Premake/Bin"

    if platform.system() == "Windows":
        premakeExe = Path(f"{premakeDirectory}/premake5.exe")
    elif platform.system() == "Linux":
        premakeExe = Path(f"{premakeDirectory}/premake5")

    @classmethod
    def Validate(cls):
        if not cls.CheckIfPremakeInstalled():
            print("Premake is not installed.")
            return False

        print(f"Correct Premake located at {os.path.abspath(cls.premakeDirectory)}")
        return True

    @classmethod
    def CheckIfPremakeInstalled(cls):
        if not cls.premakeExe.exists():
            return cls.InstallPremake()

        return True

    @classmethod
    def InstallPremake(cls):
        if platform.system() == "Windows":
            premakeZipUrl = f"https://github.com/premake/premake-core/releases/download/v{cls.premakeVersion}/premake-{cls.premakeVersion}-windows.zip"
            premakePath = f"{cls.premakeDirectory}/premake-{cls.premakeVersion}-windows.zip"
        elif platform.system() == "Linux":
            premakeZipUrl = f"https://github.com/premake/premake-core/releases/download/v{cls.premakeVersion}/premake-{cls.premakeVersion}-linux.tar.gz"
            premakePath = f"{cls.premakeDirectory}/premake-{cls.premakeVersion}-linux.tar.gz"

        permissionGranted = False
        while not permissionGranted:
            reply = str(input("Premake not found. Would you like to download Premake {0:s}? [Y/N]: ".format(cls.premakeVersion))).lower().strip()[:1]
            if reply == 'n':
                return False
            permissionGranted = (reply == 'y')

        print("Downloading {0:s} to {1:s}".format(premakeZipUrl, premakePath))
        Utils.DownloadFile(premakeZipUrl, premakePath)
        print("Extracting", premakePath)
        Utils.UnzipFile(premakePath, deleteZipFile=True)
        print(f"Premake {cls.premakeVersion} has been downloaded to '{cls.premakeDirectory}'")

        if platform.system() == "Linux":
            st = os.stat(cls.premakeExe)
            os.chmod(cls.premakeExe, st.st_mode | stat.S_IEXEC)

        premakeLicensePath = f"{cls.premakeDirectory}/LICENSE.txt"
        print("Downloading {0:s} to {1:s}".format(cls.premakeLicenseUrl, premakeLicensePath))
        Utils.DownloadFile(cls.premakeLicenseUrl, premakeLicensePath)
        print(f"Premake License file has been downloaded to '{cls.premakeDirectory}'")

        return True
