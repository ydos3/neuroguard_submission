@echo off
color 0A
echo ========================================================
echo NEUROGUARD KERNEL DRIVER - QUICK START GUIDE
echo ========================================================
echo.
echo Welcome! This project is a Linux Kernel Device Driver.
echo Because it interacts directly with the Linux Kernel, it CANNOT run on Windows.
echo.
echo To test this code and see the live demo, you need a Linux environment.
echo (For example: an AWS EC2 Ubuntu instance, Google Cloud Compute, or DigitalOcean Droplet).
echo.
echo INSTRUCTIONS:
echo 1. Copy this entire 'neuroguard' folder to your Linux Cloud VM.
echo 2. Open a terminal on your Linux VM and navigate to the folder.
echo 3. Run the automated deployment script by typing:
echo.
echo    chmod +x cloud_deploy.sh
echo    ./cloud_deploy.sh
echo.
echo The script will automatically install dependencies, build the driver,
echo load it into the kernel, and run the live anomaly detection demo!
echo.
echo All comments have been stripped from the source code as requested.
echo The code is clean, stable, and ready for submission.
echo.
pause
