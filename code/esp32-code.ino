#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// Access Point Konfiguration
const char* ap_ssid = "Zeitmesssystem";    // Name des Access Points
const char* ap_password = "12345678";      // Passwort für den Access Point (mind. 8 Zeichen)

// Webserver-Instanz
WebServer server(80);

// Pin für den Button
const int buttonPin = 15; // GPIO-Pin für den Button
bool buttonPressed = false; // Zustand des Buttons
unsigned long startTime = 0; // Startzeit der Messung
unsigned long lapTime = 0; // Gemessene Rundenzeit
bool newLapAvailable = false; // Flag, ob eine neue Rundenzeit verfügbar ist
unsigned long buttonPressStartTime = 0; // Zeitpunkt, zu dem der Knopf gedrückt wurde
bool isTiming = false; // Flag, ob die Zeitmessung aktiv ist
unsigned long lastStopTime = 0; // Zeitpunkt, zu dem die Zeitmessung zuletzt gestoppt wurde
const unsigned long minTimingDuration = 5000; // Mindestdauer der Zeitmessung (5 Sekunden)
const unsigned long cooldownDuration = 5000; // Cooldown-Zeit nach Stoppen (5 Sekunden)

// TFT-Display
#define TFT_CS   12  // Chip Select
#define TFT_DC   5   // Data/Command
#define TFT_RST  4   // Reset
#define TFT_LED  27  // Hintergrundbeleuchtung
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Globale Variablen für den Gruppennamen und die ausgewählte Zelle
String selectedGroup = "";
int selectedRound = -1;

// HTML-Webseite mit JS für Updates (automatisches Speichern)
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
    <link rel="stylesheet" href="/style.css">
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Zeitmesssystem für Autonome Fahrzeuge</title>
    <style>
        /* Allgemeine Layout-Einstellungen */
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Roboto', sans-serif;
            background-color: #f4f7fa;
            color: #333;
            padding: 40px 20px;
        }

        .container {
            max-width: 1000px;
            margin: 0 auto;
            background-color: white;
            padding: 30px;
            border-radius: 12px;
            box-shadow: 0 5px 15px rgba(0, 0, 0, 0.1);
        }

        /* Header */
        header {
            text-align: center;
            margin-bottom: 40px;
        }

        header h1 {
            font-size: 2.5rem;
            color: #333;
            font-weight: 700;
            letter-spacing: 2px;
        }

        /* Abschnitt für Gruppen- und Rundenverwaltung */
        section {
            margin-bottom: 35px;
        }

        h2 {
            font-size: 1.6rem;
            color: #333;
            margin-bottom: 20px;
            font-weight: 600;
        }

        /* Input-Gruppe für Textfelder und Buttons */
        .input-group {
            display: flex;
            justify-content: space-between;
            gap: 20px;
            margin-bottom: 30px;
        }

        .input-group input {
            padding: 12px;
            font-size: 1rem;
            border: 2px solid #ddd;
            border-radius: 8px;
            width: 48%;
            transition: border 0.3s ease;
        }

        .input-group input:focus {
            border-color: #4C8BF5;
            outline: none;
        }

        .input-group button {
            padding: 12px 25px;
            font-size: 1rem;
            background-color: #4C8BF5;
            color: white;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: background-color 0.3s ease;
        }

        .input-group button:hover {
            background-color: #3972D5;
        }

        /* Tabelle */
        table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 20px;
        }

        th, td {
            padding: 15px;
            text-align: center;
            font-size: 1rem;
        }

        td {
            background-color: #f9f9f9;
            border-bottom: 1px solid #ddd;
            border-radius: 8px;
        }

        td input {
            padding: 8px;
            font-size: 1rem;
            width: 100%;
            border: 2px solid #ddd;
            border-radius: 8px;
            transition: border 0.3s ease;
        }

        td input:focus {
            border-color: #4C8BF5;
            outline: none;
        }

        /* Auswertungsbereich */
        .evaluation {
            background-color: #f0f8ff;
            padding: 20px;
            border-radius: 8px;
            margin-top: 30px;
        }

        .evaluation h3 {
            color: #2c5282;
            margin-bottom: 15px;
        }

        .ranking-table {
            width: 100%;
            margin-top: 15px;
        }

        .ranking-table th {
            background-color: #4C8BF5;
            color: white;
        }

        .ranking-table tr:nth-child(even) {
            background-color: #f0f7ff;
        }

        /* Fußzeile */
        footer {
            text-align: center;
            font-size: 0.9rem;
            color: #888;
            margin-top: 40px;
        }

        /* Responsives Design */
        @media (max-width: 768px) {
            .input-group {
                flex-direction: column;
                align-items: center;
            }

            .input-group input,
            .input-group button {
                width: 100%;
            }

            table th,
            table td {
                font-size: 0.9rem;
            }
        }

        /* Hover-Effekt für Tabellenzellen */
        #timeTable tbody td:first-child {
            transition: background-color 0.3s, color 0.3s;
        }

        #timeTable tbody td:first-child:hover {
            background-color: #f0f0f0;
            color: #007bff;
            font-weight: bold;
        }

        /* Gruppenspezifische Aktionen */
        .group-actions {
            display: none;
            cursor: pointer;
        }

        .group-container {
            display: flex;
            align-items: center;
            gap: 5px;
        }

        /* Gruppenname ist nun nicht mehr klickbar */
        .group-name {
            cursor: default;
        }

        /* Disabled Input */
        .disabled-input {
            background-color: #e9ecef !important;
            color: #495057 !important;
        }

        /* Button-Styles */
        .action-buttons {
            display: flex;
            gap: 15px;
            margin-top: 20px;
        }

        .btn-evaluate {
            padding: 12px 25px;
            font-size: 1rem;
            background-color: #4C8BF5;
            color: white;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: background-color 0.3s ease;
        }

        .btn-reset {
            padding: 12px 25px;
            font-size: 1rem;
            background-color: #4C8BF5;
            color: white;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: background-color 0.3s ease;
        }

        .btn-load {
            padding: 12px 25px;
            font-size: 1rem;
            background-color: #4C8BF5;
            color: white;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: background-color 0.3s ease;
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>Zeitmessung</h1>
        </header>

        <!-- Tabelle zur Anzeige der Gruppen und Rundenzeiten -->
        <section class="table-container">
            <table id="timeTable">
                <thead>
                    <tr>
                        <th>Gruppenname</th>
                    </tr>
                </thead>
                <tbody>
                    <!-- Gruppen werden hier angezeigt -->
                </tbody>
            </table>
        </section>

        <!-- Eingabeformular für neue Gruppen -->
        <section class="group-management">
            <div class="input-group">
                <input type="text" id="groupName" placeholder="Gruppenname" required>
                <button class="btn" onclick="addGroup()">Gruppe hinzufügen</button>
            </div>
        </section>

        <!-- Eingabeformular für Anzahl der Runden -->
        <section class="round-management">
            <div class="input-group">
                <input type="number" id="roundCount" placeholder="Anzahl Runden" required min="1">
                <button class="btn" onclick="generateTable()">Runden hinzufügen</button>
            </div>
        </section>

        <!-- Action Buttons -->
        <section class="actions">
            <div class="action-buttons">
                <button class="btn btn-evaluate" onclick="evaluateTimes()">Auswerten</button>
                <button class="btn btn-reset" onclick="resetTable()">Tabelle zurücksetzen</button>
            </div>
        </section>

        <!-- Auswertungsbereich (initial versteckt) -->
        <section class="evaluation" id="evaluationSection" style="display: none;">
            <h3>Auswertung</h3>
            <div id="evaluationResults"></div>
        </section>
    </div>

    <script>
        let groups = []; // Array zur Speicherung der Gruppen
        let roundCount = 0; // Anzahl der Runden
        let lapTimes = {}; // Objekt zur Speicherung der Rundenzeiten pro Gruppe
        let selectedCell = null; // Aktuell ausgewähltes Feld

        // Funktion zum Speichern der Daten im Local Storage
        function saveData() {
            const data = {
                groups: groups,
                lapTimes: lapTimes,
                roundCount: roundCount
            };
            localStorage.setItem('timeMeasurementData', JSON.stringify(data));
            console.log('Daten automatisch gespeichert');
        }

        // Funktion zum Laden der Daten beim Start
        function loadInitialData() {
            const savedData = localStorage.getItem('timeMeasurementData');
            if (savedData) {
                const data = JSON.parse(savedData);
                groups = data.groups || [];
                lapTimes = data.lapTimes || {};
                roundCount = data.roundCount || 0;
                
                document.getElementById('roundCount').value = roundCount;
                updateTable();
                console.log('Daten beim Start geladen');
            }
        }

        // Funktion zum Hinzufügen einer neuen Gruppe
        function addGroup() {
            let name = document.getElementById('groupName').value.trim();
            if (!name) return;
            if (groups.some(g => g.toLowerCase() === name.toLowerCase())) {
                alert("Diese Gruppe existiert bereits!");
                return;
            }
            groups.push(name);
            lapTimes[name] = Array(roundCount).fill(""); // Initialisiere Rundenzeiten für die neue Gruppe
            updateTable();
            document.getElementById('groupName').value = '';
            saveData(); // Daten nach Änderung speichern
        }

        // Funktion zur Erstellung der Tabelle mit Rundenanzahl
        function generateTable() {
            roundCount = parseInt(document.getElementById('roundCount').value);
            updateTable();
            saveData(); // Daten nach Änderung speichern
        }

        // Funktion zur Aktualisierung der Tabelle
        function updateTable() {
            let table = document.getElementById('timeTable');
            let thead = table.querySelector('thead tr');
            let tbody = table.querySelector('tbody');

            // Überschriften aktualisieren (Gruppenname + Rundenüberschriften)
            thead.innerHTML = '<th>Gruppenname</th>';
            for (let i = 1; i <= roundCount; i++) {
                let th = document.createElement('th');
                th.textContent = `Runde ${i}`;
                thead.appendChild(th);
            }

            // Tabelleninhalt aktualisieren (Gruppen mit Rundenzeiten)
            tbody.innerHTML = ''; // Tabelle leeren
            groups.forEach((group, index) => {
                let row = document.createElement('tr');
                let nameCell = document.createElement('td');

                // Container für den Namen und die Icons
                let container = document.createElement('div');
                container.classList.add('group-container');

                // Gruppenname anzeigen (nun nicht klickbar)
                let nameSpan = document.createElement('span');
                nameSpan.textContent = group;
                nameSpan.classList.add('group-name');
                container.appendChild(nameSpan);

                // Bearbeiten-Symbol (Stift Emoji)
                let editIcon = document.createElement('span');
                editIcon.textContent = "✏️";
                editIcon.classList.add('edit-icon', 'group-actions');
                editIcon.dataset.index = index;
                container.appendChild(editIcon);

                // Löschen-Symbol (❌ Emoji)
                let deleteIcon = document.createElement('span');
                deleteIcon.textContent = "❌";
                deleteIcon.classList.add('delete-icon', 'group-actions');
                deleteIcon.dataset.index = index;
                deleteIcon.style.marginLeft = "10px";
                container.appendChild(deleteIcon);

                nameCell.appendChild(container);
                row.appendChild(nameCell);

                // Eingabefelder für die Rundendaten erstellen
                for (let i = 0; i < roundCount; i++) {
                    let lapCell = document.createElement('td');
                    let input = document.createElement('input');
                    input.type = 'text';
                    input.placeholder = 'Zeit';
                    input.value = lapTimes[group][i] || ""; // Vorhandene Rundenzeit wiederherstellen

                    // Deaktiviere das Feld, wenn bereits eine Zeit eingetragen wurde
                    if (lapTimes[group][i]) {
                        input.disabled = true;
                        input.classList.add('disabled-input');
                    }

                    input.addEventListener('click', () => {
                        // Markiere das ausgewählte Feld nur, wenn es nicht deaktiviert ist
                        if (!input.disabled) {
                            if (selectedCell) {
                                selectedCell.style.backgroundColor = ""; // Alte Auswahl zurücksetzen
                            }
                            selectedCell = input;
                            selectedCell.style.backgroundColor = "#e0f7fa"; // Neue Auswahl markieren

                            // Sende den Gruppennamen und die Rundenindex an den Arduino
                            let groupName = selectedCell.closest('tr').querySelector('.group-name').textContent;
                            let roundIndex = Array.from(selectedCell.parentElement.parentElement.children).indexOf(selectedCell.parentElement) - 1;
                            sendSelectedCell(groupName, roundIndex);
                        }
                    });

                    input.addEventListener('input', () => {
                        lapTimes[group][i] = input.value; // Rundenzeit speichern
                        saveData(); // Daten nach Änderung speichern
                    });

                    lapCell.appendChild(input);
                    row.appendChild(lapCell);
                }

                tbody.appendChild(row);
            });

            applyHoverEffect();
            applyClickEffect();
        }

        // Funktion für Hover-Effekte
        function applyHoverEffect() {
            let containers = document.querySelectorAll(".group-container");
            containers.forEach(container => {
                container.addEventListener("mouseenter", function () {
                    container.querySelectorAll(".group-actions").forEach(icon => icon.style.display = "inline");
                });
                container.addEventListener("mouseleave", function () {
                    container.querySelectorAll(".group-actions").forEach(icon => icon.style.display = "none");
                });
            });
        }

        // Funktion für Klick-Effekte (Bearbeiten und Löschen)
        function applyClickEffect() {
            // Bearbeiten der Gruppe über das Stift-Icon
            document.querySelectorAll(".edit-icon").forEach(icon => {
                icon.addEventListener("click", function (event) {
                    event.stopPropagation();
                    let index = icon.dataset.index;
                    let groupNameSpan = icon.parentElement.querySelector(".group-name");
                    let originalText = groups[index];

                    let input = document.createElement("input");
                    input.type = "text";
                    input.value = originalText;
                    input.classList.add("edit-input");

                    groupNameSpan.textContent = "";
                    groupNameSpan.appendChild(input);
                    input.focus();

                    input.addEventListener("blur", function () {
                        let newName = input.value.trim();
                        if (newName && groups.some((g, i) => g.toLowerCase() === newName.toLowerCase() && i != index)) {
                            alert("Diese Gruppe existiert bereits!");
                            updateTable();
                            return;
                        }
                        if (newName) {
                            // Rundenzeiten für die umbenannte Gruppe speichern
                            lapTimes[newName] = lapTimes[originalText];
                            delete lapTimes[originalText];
                            groups[index] = newName;
                            saveData(); // Daten nach Änderung speichern
                        }
                        updateTable();
                    });

                    input.addEventListener("keydown", function (event) {
                        if (event.key === "Enter") {
                            input.blur();
                        }
                    });
                });
            });

            // Löschen der Gruppe über das ❌-Icon
            document.querySelectorAll(".delete-icon").forEach(icon => {
                icon.addEventListener("click", function (event) {
                    event.stopPropagation();
                    let index = icon.dataset.index;
                    if (confirm("Möchtest du diese Gruppe wirklich löschen?")) {
                        delete lapTimes[groups[index]]; // Rundenzeiten der Gruppe löschen
                        groups.splice(index, 1);
                        saveData(); // Daten nach Änderung speichern
                        updateTable();
                    }
                });
            });
        }

        // Funktion zum Senden des Gruppennamens und der ausgewählten Zelle an den Arduino
        function sendSelectedCell(groupName, roundIndex) {
            fetch('/setSelectedCell?group=' + encodeURIComponent(groupName) + '&round=' + roundIndex)
                .then(response => response.text())
                .then(data => console.log(data))
                .catch(error => console.error('Fehler:', error));
        }

        // Funktion zum Abrufen der Rundenzeit vom ESP32
        function getLapTime() {
            fetch('/getTime')
                .then(response => response.text())
                .then(data => {
                    if (data !== "0.000" && selectedCell && !selectedCell.disabled) {
                        selectedCell.value = data; // Zeit in das ausgewählte Feld eintragen
                        let groupName = selectedCell.closest('tr').querySelector('.group-name').textContent;
                        let roundIndex = Array.from(selectedCell.parentElement.parentElement.children).indexOf(selectedCell.parentElement) - 1;
                        lapTimes[groupName][roundIndex] = data; // Rundenzeit speichern
                        selectedCell.disabled = true; // Deaktiviere das Feld nach dem Eintragen
                        selectedCell.classList.add('disabled-input');
                        saveData(); // Daten nach Änderung speichern
                    }
                });
        }

        // Funktion zur Auswertung der Zeiten
        function evaluateTimes() {
            let evaluationSection = document.getElementById('evaluationSection');
            let resultsDiv = document.getElementById('evaluationResults');
            
            // Ergebnisse zurücksetzen
            resultsDiv.innerHTML = '';
            
            // Array für die Durchschnittszeiten erstellen
            let averages = [];
            
            // Durchschnittszeiten berechnen
            groups.forEach(group => {
                let validTimes = lapTimes[group].filter(time => time && time !== "").map(time => parseFloat(time));
                if (validTimes.length > 0) {
                    let sum = validTimes.reduce((a, b) => a + b, 0);
                    let avg = sum / validTimes.length;
                    averages.push({
                        group: group,
                        average: avg.toFixed(3),
                        bestTime: Math.min(...validTimes).toFixed(3),
                        totalTime: sum.toFixed(3),
                        count: validTimes.length
                    });
                }
            });
            
            // Nach Durchschnittszeit sortieren
            averages.sort((a, b) => a.average - b.average);
            
            // Ergebnisse anzeigen
            if (averages.length > 0) {
                // Rangliste erstellen
                let rankingTable = document.createElement('table');
                rankingTable.className = 'ranking-table';
                
                // Tabellenkopf
                let thead = document.createElement('thead');
                thead.innerHTML = `
                    <tr>
                        <th>Platz</th>
                        <th>Gruppe</th>
                        <th>Beste Zeit (s)</th>
                        <th>Durchschnitt (s)</th>
                        <th>Gesamtzeit (s)</th>
                        <th>Anzahl Runden</th>
                    </tr>
                `;
                rankingTable.appendChild(thead);
                
                // Tabelleninhalt
                let tbody = document.createElement('tbody');
                averages.forEach((item, index) => {
                    let row = document.createElement('tr');
                    row.innerHTML = `
                        <td>${index + 1}</td>
                        <td>${item.group}</td>
                        <td>${item.bestTime}</td>
                        <td>${item.average}</td>
                        <td>${item.totalTime}</td>
                        <td>${item.count}</td>
                    `;
                    tbody.appendChild(row);
                });
                rankingTable.appendChild(tbody);
                
                resultsDiv.appendChild(rankingTable);
                
                // Auswertungsbereich anzeigen
                evaluationSection.style.display = 'block';
            } else {
                resultsDiv.innerHTML = '<p>Keine Zeiten zum Auswerten vorhanden.</p>';
                evaluationSection.style.display = 'block';
            }
        }

        // Funktion zum Zurücksetzen der Tabelle
        function resetTable() {
            if (confirm("Möchtest du wirklich alle Daten zurücksetzen? Dies kann nicht rückgängig gemacht werden.")) {
                groups = [];
                lapTimes = {};
                roundCount = 0;
                document.getElementById('groupName').value = '';
                document.getElementById('roundCount').value = '';
                document.getElementById('evaluationSection').style.display = 'none';
                localStorage.removeItem('timeMeasurementData'); // Auch aus Local Storage löschen
                updateTable();
            }
        }

        // Funktion zum Laden der Daten aus dem Local Storage
        function loadData() {
            const savedData = localStorage.getItem('timeMeasurementData');
            if (savedData) {
                const data = JSON.parse(savedData);
                groups = data.groups || [];
                lapTimes = data.lapTimes || {};
                roundCount = data.roundCount || 0;
                
                document.getElementById('roundCount').value = roundCount;
                updateTable();
                alert('Daten erfolgreich geladen!');
            } else {
                alert('Keine gespeicherten Daten gefunden!');
            }
        }

        // Beim Laden der Seite initiale Daten laden
        window.addEventListener('DOMContentLoaded', (event) => {
            loadInitialData();
        });

        // Intervall zum regelmäßigen Abrufen der Rundenzeit
        setInterval(getLapTime, 1000);
    </script>
</body>
</html>
)rawliteral";

// Webserver-Handler für HTML-Seite
void handleRoot() {
    server.send(200, "text/html", htmlPage);
}

// Webserver-Handler für Zeitmessung
void handleGetTime() {
    if (newLapAvailable) {
        // Sende die Zeit im Format "Sekunden.Millisekunden"
        server.send(200, "text/plain", String(lapTime / 1000.0, 3));
        newLapAvailable = false; // Flag zurücksetzen
    } else {
        server.send(200, "text/plain", "0.000"); // Keine neue Rundenzeit verfügbar
    }
}

// Webserver-Handler für die ausgewählte Zelle
void handleSetSelectedCell() {
    if (server.hasArg("group") && server.hasArg("round")) {
        selectedGroup = server.arg("group");
        selectedRound = server.arg("round").toInt();
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Fehlende Parameter");
    }
}

// Funktion zur Anzeige der IP-Adresse
void showIPAddress() {
    tft.fillRect(0, 220, 320, 20, ILI9341_BLACK); // Bereich für IP-Adresse
    tft.setCursor(10, 200);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    tft.print("Verbinde zu: ");
    tft.print(ap_ssid);
    tft.setCursor(10, 215);
    tft.print("IP: ");
    tft.print(WiFi.softAPIP());
}

// Funktion zum Aktualisieren des Displays
void updateDisplay(String groupName, unsigned long currentTime) {
    static String lastGroupName = ""; // Speichert den letzten Gruppenname
    static unsigned long lastTime = 0; // Speichert die letzte Zeit

    // Korrigiere die Zeit um 7 ms
    unsigned long correctedTime = currentTime + 8;

    // Gruppenname aktualisieren (nur wenn er sich ändert)
    if (groupName != lastGroupName) {
        tft.fillRect(10, 10, 240, 30, ILI9341_BLACK); // Größerer Bereich für den Gruppenname
        tft.setCursor(10, 10);
        tft.setTextColor(ILI9341_WHITE);
        tft.setTextSize(2);
        tft.print("Gruppe: ");
        tft.println(groupName);
        lastGroupName = groupName; // Aktualisiere den letzten Gruppenname
    }

    // Zeit aktualisieren (nur wenn sie sich ändert)
    if (correctedTime != lastTime) {
        tft.fillRect(10, 50, 240, 40, ILI9341_BLACK); // Größerer Bereich für die Zeit-Anzeige
        tft.setCursor(10, 50);
        tft.setTextColor(ILI9341_GREEN);
        tft.setTextSize(3);
        tft.print("Zeit: ");
        tft.print(correctedTime / 1000.0, 3); // Zeit in Sekunden mit Millisekunden anzeigen
        tft.println(" s");
        lastTime = correctedTime; // Aktualisiere die letzte Zeit
    }
}

void setup() {
    Serial.begin(115200);
    delay(3000);
    pinMode(buttonPin, INPUT_PULLUP); // Button-Pin als Eingang setzen
    pinMode(TFT_LED, OUTPUT); // Hintergrundbeleuchtung aktivieren
    digitalWrite(TFT_LED, HIGH); // Hintergrundbeleuchtung einschalten

    // TFT-Display initialisieren
    tft.begin();
    tft.setRotation(3); // Ausrichtung des Displays anpassen
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    
    tft.setCursor(10, 10);
    tft.println("Erstelle Access Point...");

    // Access Point starten
    WiFi.softAP(ap_ssid, ap_password);
    IPAddress myIP = WiFi.softAPIP();
    
    Serial.print("Access Point gestartet: ");
    Serial.println(ap_ssid);
    Serial.print("IP-Adresse: ");
    Serial.println(myIP);

    // IP-Adresse auf dem Display anzeigen
    showIPAddress();

    // Webserver-Routen definieren
    server.on("/", handleRoot);
    server.on("/getTime", handleGetTime);
    server.on("/setSelectedCell", handleSetSelectedCell);
    server.begin();
}

void loop() {
    server.handleClient();

    // Button-Zustand abfragen
    if (digitalRead(buttonPin) == LOW) { // Button gedrückt
        if (!buttonPressed) {
            buttonPressStartTime = millis(); // Zeitpunkt des Knopfdrucks speichern
            buttonPressed = true;
        } else {
            // Überprüfen, ob der Knopf mindestens 500 ms gedrückt wurde
            if (millis() - buttonPressStartTime >= 100) {
                if (!isTiming) {
                    // Zeitmessung starten (nur wenn kein Cooldown aktiv ist)
                    if (millis() - lastStopTime >= cooldownDuration) {
                        startTime = millis(); // Startzeit setzen
                        isTiming = true;
                        Serial.println("Zeitmessung gestartet!");
                    } else {
                        Serial.println("Cooldown aktiv! Zeitmessung kann noch nicht gestartet werden.");
                    }
                } else {
                    // Zeitmessung stoppen (nur wenn mindestens 5 Sekunden vergangen sind)
                    if (millis() - startTime >= minTimingDuration) {
                        lapTime = millis() - startTime; // Rundenzeit berechnen
                        isTiming = false;
                        lastStopTime = millis(); // Zeitpunkt des Stoppens speichern
                        newLapAvailable = true; // Neue Rundenzeit verfügbar
                        Serial.print("Rundenzeit: ");
                        Serial.print(lapTime);
                        Serial.println(" ms");
                    } else {
                        Serial.println("Mindestdauer von 5 Sekunden nicht erreicht!");
                    }
                }
                buttonPressed = false; // Reset für nächsten Tastendruck
            }
        }
    } else {
        buttonPressed = false; // Reset für nächsten Tastendruck
    }

    // Aktualisiere das Display, wenn die Zeitmessung aktiv ist
    if (isTiming && selectedGroup != "") {
        unsigned long currentTime = millis() - startTime;
        updateDisplay(selectedGroup, currentTime);
    }
}
