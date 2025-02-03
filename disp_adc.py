import serial
import re
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from scipy import interpolate

# Initialisiere serielle Verbindung
ser = serial.Serial('COM7', 115200, timeout=1)

transfer_size = 1024
sample_rate = 50000

# Initialisiere Datenspeicher
voltages = [0.0] * transfer_size
goertzels = [0.0] * transfer_size
buffer = ""

# Erstelle Plots
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))
plot_line, = ax1.plot(range(transfer_size), voltages, '-')
goertzel_line, = ax1.plot(range(transfer_size), goertzels, '-')


ax1.set_xlim(0, transfer_size)
ax1.set_ylim(0, 1.1)
ax1.set_title('ADC Input Voltage')
ax1.set_xlabel('Sample Number')
ax1.set_ylabel('Voltage (V)')

time_lines = np.arange(0, 1024, 20.48) 
ax1.vlines(time_lines, -2, 1, linestyles='dashed', colors='gray', alpha=0.5)

# FFT Plot
fft_line, = ax2.plot([], [],'-')
ax2.set_xlim(0, 50)  # Nyquist-Frequenz ist die Hälfte der Abtastrate
ax2.set_ylim(-1, 1)
ax2.set_title('FFT of ADC Input Voltage')
ax2.set_xlabel('Frequency (Hz)')
ax2.set_ylabel('Magnitude')

# Erstelle vertikale Linien für jedes 1kHz Intervall
freq_lines = np.arange(0, sample_rate/2, 1000)  # Von 0 bis 50kHz in 1kHz Schritten
ax2.vlines(freq_lines, -2, 1, linestyles='dashed', colors='gray', alpha=0.5)


# Funktion zum Aktualisieren der Plots
def update_plot(frame):
    global buffer
    global voltages
    global goertzels
    global transfer_size
    global sample_rate
    try:
        if ser.in_waiting:
            data = ser.read(ser.in_waiting).decode('utf-8')        
            buffer += data
            lines = buffer.split('\n')
            buffer = lines.pop()
            
            for line in lines:   
                #match = re.search(r'ADC Count \[(\d+)\] = 0x[0-9a-fA-F]+, ADC Input Voltage = (\d+\.\d+) V', line)
                #match = re.search(r'ADC Count \[(\d+)\] = (\d+), ADC Input Voltage = (\d+\.\d+) V', line)
                match = re.search(r'ADC Count \[(-?\d+)\] = (-?\d+), ADC Input Voltage = (-?\d+\.-?\d+) V Goertzel = (-?\d+)', line)
                
                if match:
                    sample_num = int(match.group(1))
                    raw_value =  int(match.group(2))
                    voltage = float(match.group(3))
                    goertzel = float(match.group(4)) 
                    

                    # Überprüfen Sie, ob sample_num innerhalb des gültigen Bereichs liegt
                    if 0 <= sample_num < transfer_size:
                        voltages[sample_num] = voltage
                        goertzels[sample_num] = goertzel/2048
                    else:
                        print(f"Warnung: Sample Nummer {sample_num} außerhalb des gültigen Bereichs (0-{transfer_size-1})")
                                                                                
                    voltages[sample_num] = voltage
                    goertzels[sample_num] = goertzel/2048
                    # print(f"{match} : {sample_num} : {raw_value}")
                    
                    #plot_line.set_ydata(voltages)
                    #ax1.set_ylim(0, max(voltages) + 0.1) 
                    ax1.relim() 
                    ax1.autoscale_view() 

            #print(f"{voltages}")
            plot_line.set_ydata(voltages)
            goertzel_line.set_ydata(goertzels)

            # Berechne FFT
            hanning_window = np.hanning(len(voltages))
            windowed_voltages = voltages * hanning_window
            fft_result = np.fft.fft(windowed_voltages)
            fft_freq = np.fft.fftfreq(len(voltages), d=1/sample_rate)  # Abtastrate von 100 kHz
            
            #fft_mag = np.abs(fft_result)[:len(fft_result)//2] # linear
            fft_mag = 20 * np.log10(np.abs(fft_result[:len(fft_result)//2]) + 1e-10) # log
            
            fft_mag[0:2] = 0.00000001
            
            # Normalisiere die Magnitude
            fft_mag = fft_mag / np.max(fft_mag)
            
            # Entferne DC-Komponente
            fft_mag[0] = 0

            # Originale Frequenzpunkte und Magnitudenwerte
            x_orig = fft_freq[:len(fft_freq)//2]
            y_orig = fft_mag

            # Neue Frequenzpunkte mit höherer Auflösung, aber im exakt gleichen Bereich
            x_new = np.linspace(x_orig[0], x_orig[-1], 1000)  # 1000 Punkte zwischen Start und Ende der originalen Frequenzen

            # Kubische Interpolation
            f_cubic = interpolate.interp1d(x_orig, y_orig, kind='cubic')
            y_new = f_cubic(x_new)


            # Gleitender Mittelwert für Glättung
            window_size = 15  # Fenstergröße für den Mittelwert anpassen
            kernel = np.ones(window_size) / window_size
            y_smoothed = np.convolve(y_new, kernel, mode='same')

            # Plot mit interpolierten Werten
            fft_line.set_data(x_new, y_smoothed) ## interpolierte daten
            #fft_line.set_data(fft_freq[:len(fft_freq)//2], fft_mag) # original daten
            
            ax2.set_ylim(-2.1, 1.1)
            ax2.set_xlim(0, 50000)  # Begrenzen Sie die x-Achse auf 0-10kHz
            
        
    except serial.SerialException as e:
        print(f"Fehler bei der seriellen Kommunikation: {e}")
    except UnicodeDecodeError:
        print("Warnung: Ungültige Daten empfangen, werden ignoriert.")
    except Exception as e:
        print(f"Unerwarteter Fehler: {e}")
        
    return plot_line, fft_line

# Erstelle Animation
ani = FuncAnimation(fig, update_plot, frames=None, interval=100, blit=False)

plt.tight_layout()
plt.show()

# Schließe serielle Verbindung, wenn das Plot geschlossen wird
plt.close()
ser.close()
