clc;
clear all;
wgs84 = wgs84Ellipsoid;

% Coordinate Padova (Liceo Fermi)
lat0 = 45.39575;
lon0 = 11.87974;
alt0 = 12;

% Coordinate della ISS
filename = 'C:\Users\Utente\Downloads\Progettino\IssMATLAB.txt'; %MATLAB takes orbit's datas from the GMAT file

data = readmatrix(filename,'FileType','text');
T   = data(2:end,10);
X   = data(2:end,5).*1000; 
Y   = data(2:end,6).*1000;
Z   = data(2:end,7).*1000;

[lat_iss, lon_iss, alt_iss] = ecef2geodetic(wgs84, X, Y, Z);

[az, elev, range] = geodetic2aer(lat_iss, lon_iss, alt_iss, lat0, lon0, alt0, wgs84);

%Plot azimuth ed elevazione nel tempo
figure;
plot(T, az, 'r', T, elev, 'b');
xlabel('Time (seconds)');
ylabel('Degrees');
legend('Azimuth','Elevation');
title('Tracciamento ISS rispetto a Padova');
grid on;

%Creazione file csv per Arduino
dati = [T az elev];
dati(:,1) = round(dati(:,1)); % Tempo intero
dati(:,2:3) = round(dati(:,2:3), 1); % 2 decimali per Az/El
writematrix(dati, 'IssArdu.csv', 'Delimiter', ',');