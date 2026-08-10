[Instructions.txt](https://github.com/user-attachments/files/30898591/Instructions.txt)
[ISS Tracker.pdf](https://github.com/user-attachments/files/30898575/ISS.Tracker.pdf)

Copy the TLEs from [https://live.ariss.org/tle/](https://live.ariss.org/tle/) and make a note of the TLE Epoch.
Paste the TLEs into the file "TLE_ISS.txt".
In GMAT, open the "Propagatore" script.
Insert the TLE Epoch corresponding to the TLE pasted into "TLE_ISS.txt" where indicated (seventh line).
Press "Save, Sync, Run".
Open the file Convertitore.m and run it.
You will now have obtained a CSV file containing the elapsed seconds relative to the TLE Epoch, azimuth, and elevation with respect to Padua.
If you want them relative to another location, you would need to modify the terrestrial reference coordinates in Convertitore.m.
In the Arduino program "Elaboratore.ino", enter the same Epoch written in GMAT (line 23).
Copy the CSV file created by MATLAB ("IssArdu.csv") onto the microSD card, replacing any existing file. Then eject the card from the computer and insert it into the Arduino.
Run it.

IF SOMETHING DOES NOT WORK, it could be due to:

* TLE and/or Epoch not being updated/consistent
* Files saved in the wrong folders (in this case, update the destinations and/or file search paths in GMAT, MATLAB, and Arduino IDE)
* Missing required libraries in MATLAB and/or Arduino IDE
* Check whether you are at UTC +2 or UTC +1
