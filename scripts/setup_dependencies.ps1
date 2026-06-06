# Setup script for Ubuntu 22.04 / WSL2
# Run from a bash shell, not PowerShell
Write-Host "This project targets Linux. Please run on Ubuntu 22.04 or WSL2."
Write-Host "Setup command:"
Write-Host "  sudo apt-get update && sudo apt-get install -y cmake ninja-build g++-12 libssl-dev libboost-all-dev zlib1g-dev nlohmann-json3-dev"