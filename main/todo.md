Roadmap

1. Verhalten festlegen
Lege die Einstellung eindeutig fest, zum Beispiel:


Ablauf:
Standardmäßig: Webserver immer an
Im frontend kann eingestellt werden
Wenn Option angewählt -> Speicherung im NVS -> Direkte Abschaltung des Webservers falls Client verbunden ist
Wird der Client getrennt muss der Webserver wieder gestartet werden
Abwählen möglich falls kein client verbunden ist sonst via factory reset



2. Einstellung laden und speichern
Füge die boolesche Einstellung in die bestehende Konfigurations-/NVS-Verwaltung ein. Beim Start:

Nicht vorhandener NVS-Wert → false.
Vorhandener Wert → gespeicherten Wert verwenden.
Andere NVS-Fehler → sichtbar protokollieren bzw. als Fehler behandeln, nicht stillschweigend wie „false“ behandeln.
Prüfe für das Muster zunächst die vorhandene NVS-Verwaltung, etwa status_light_nvs, und entscheide, ob die neue, funktionsfremde Option dort wirklich hingehört. Falls nicht, lege sie in einer passenden zentralen App- oder Webserver-Einstellungsverwaltung ab.

3. Webserver-Einstellung anbieten
Ergänze einen GET-/POST-Endpunkt und einen Schalter in der Oberfläche:

GET liefert den gespeicherten Wert.
POST validiert und speichert ihn.
Die Oberfläche zeigt den tatsächlichen gespeicherten Zustand an.
Nutze dafür die vorhandenen Callback-Muster zwischen Webserver und Anwendung, statt NVS direkt aus dem Frontend-HTTP-Handler anzusprechen. Der Webserver wird in main.cpp gestartet; die Callback-Typen und registrierten Routen liegen im Webserver-Bereich.

4. TCP-Zustände als Ereignisse melden
In proxy_transport.cpp wird ein Client angenommen; bei recv() <= 0 wird die Trennung erkannt. Diese Stellen sollten nur verbundener/getrennter Zustand melden — nicht direkt den HTTP-Server stoppen.

Berücksichtige außerdem den Proxy-Stopp in TcpTransport::close(): Auch ein Moduswechsel oder Reset darf den Webserver nicht abgeschaltet zurücklassen.

5. Start und Stopp im Webserver kapseln
Ergänze eine öffentliche Stop-Funktion neben esp_br_web_start(). Sie sollte den internen Server-Handle verwalten, den Server stoppen und den Handle auf NULL setzen. Der Serverzustand sollte wiederholte Start-/Stopp-Aufrufe vertragen.

Für den Neustart müssen Basis-Pfad und Callbacks weiterhin verfügbar sein. esp_br_web_start() kopiert die Callback-Strukturen bereits in statische Variablen; prüfe, ob auch der Basis-Pfad für einen späteren Neustart sicher gespeichert wird.

6. Ereignisse zentral verarbeiten
Ein Handler oder eine kleine Steuerkomponente wertet Einstellung und TCP-Zustand gemeinsam aus:


Halte dabei fest, warum der Server gestoppt wurde. Sonst könnte ein Disconnect ihn starten, obwohl er aus einem anderen Grund gestoppt worden war.

Wichtig: Ein HTTP-Handler sollte den Server nicht direkt während der Verarbeitung seines eigenen Requests stoppen. Die Einstellungsänderung sollte entkoppelt verarbeitet werden, damit die Speicherantwort noch sauber beim Browser ankommt.

7. Prüfen
Teste mindestens diese Fälle:

Erststart ohne NVS-Wert: Option ist aus, Webserver bleibt erreichbar.
Option speichern, Neustart: Wert bleibt erhalten.
Option an, kein TCP-Client: Webserver bleibt erreichbar.
Option an, TCP-Client verbindet sich: HTTP-Webserver ist nicht mehr erreichbar.
TCP-Client trennt oder Proxy wird gestoppt: Webserver kommt zurück.
Option aus: TCP-Verbindungen schalten den Webserver nicht ab.
Wiederholte Connect-/Disconnect-Zyklen: kein doppelter Start und keine ungültigen Handles.
Der TCP-Übergang ist in TcpTransport::read(); der Webserver-Handle ist derzeit intern in esp_br_web.c. Diese beiden Verantwortlichkeiten solltest du über die Ereignis-/Steuerlogik verbinden, nicht durch direkte gegenseitige Aufrufe.