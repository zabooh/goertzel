
# brew install matplotlib
# brew install python-matplotlib
# brew intall numpy
# brew link --overwrite numpy

import matplotlib.pyplot as plt

# Listen für Input- und Output-Werte
input_values = []
output_values = []
rms_values = []

try:
    # Lese die Datei 'output.txt' ein
    with open('output.txt', 'r') as file:
        for line in file:
            # Jede Zeile enthält ein Werte-Paar, getrennt durch Leerzeichen oder Tabulator
            input_val, output_val, rms_val = map(int, line.strip().split())
            input_values.append(input_val)
            output_values.append(output_val)
            rms_values.append(rms_val)

    # Erstelle ein Diagramm
    plt.figure(figsize=(10, 6))
    plt.plot(input_values, label='Input Signal')
    plt.plot(output_values, label='Output Signal')
    plt.plot(rms_values, label='RMS Signal')
    plt.title('Input und Output Signale')
    plt.xlabel('Sample')
    plt.ylabel('Amplitude')
    plt.legend()
    plt.grid(True)

    # Zeige das Diagramm an
    plt.show()

except FileNotFoundError:
    print("Die Datei 'output.txt' wurde nicht gefunden. Stellen Sie sicher, dass sie im gleichen Verzeichnis wie dieses Skript liegt.")
except ValueError:
    print("Die Datei enthält ungültige Werte. Stellen Sie sicher, dass jede Zeile zwei ganze Zahlen enthält.")
except Exception as e:
    print(f"Ein unerwarteter Fehler ist aufgetreten: {e}")
