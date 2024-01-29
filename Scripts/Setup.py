import os
import subprocess
import platform

# Make sure everything we need for the setup is installed
from SetupPython import PythonConfiguration as PythonRequirements
PythonRequirements.Validate()

from SetupPremake import PremakeConfiguration as PremakeRequirements
from SetupVulkan import VulkanConfiguration as VulkanRequirements

os.chdir('./../') # Change from devtools/scripts directory to root
premakeInstalled = PremakeRequirements.Validate()
VulkanRequirements.Validate()

print("\nUpdating submodules...")
subprocess.call(["git", "submodule", "update", "--init", "--recursive"])

if (premakeInstalled):
    print("\nRunning premake...")
    if platform.system() == "Windows":
        subprocess.call([os.path.abspath("./Scripts/GenerateProjects-Win.bat"), "nopause"])
    if platform.system() == "Linux":
        subprocess.call([os.path.abspath("./Scripts/GenerateProjects-Linux.sh"), "nopause"])
    print("\nSetup completed!")
else:
    print("\nEngine requires Premake to generate project files!")
