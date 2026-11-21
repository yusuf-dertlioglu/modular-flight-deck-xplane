import struct
import serial
import socket
import time
import threading
import xpc

# --- KONFIGURATION ---
SERIAL_PORT = "COM3"  # Passe deinen COM-Port an
BAUD_RATE = 115200

XP_IP = "127.0.0.1"
XP_SEND_PORT = 49000     # Port, an den X-Plane Steuerbefehle erwartet
XP_LISTEN_PORT = 49001   # Port, auf dem Python Telemetrie empfängt

# Formate
PACKET_FORMAT = '<2sHhIbB?B'         # 13 Bytes + 1 Bool vom Pico
TELEMETRY_FORMAT = '<2sfffffffBBBHhIbBB' # 44 Bytes zum Pico

running = True

"""def send_dataref_to_xplane(sock_send, dataref_path, value):
    try:
        # Den Pfad exakt auf 500 Bytes null-terminiert auffüllen
        path_bytes = dataref_path.encode('ascii').ljust(500, b'\x00')
        
        # Format laut X-Plane SDK:
        # '<4sxf500s' -> 4 Bytes ('DREF') + 1 Padding-Byte ('\x00') + 4 Bytes Float + 500 Bytes String
        # Gesamtlänge: 4 + 1 + 4 + 500 = 509 Bytes
        packet = struct.pack('<4sxf500s', b'DREF', float(value), path_bytes)
        
        sock_send.sendto(packet, (XP_IP, XP_SEND_PORT))
    except Exception as e:
        print(f"[Fehler beim Senden von DataRef {dataref_path}]: {e}")"""

def send_dataref_to_xplane(client, dataref_path, value):
    """Sendet einen DataRef-Wert sauber und fehlerfrei über die XPC-Bibliothek."""
    try:
        client.sendDREF(dataref_path, float(value))
    except Exception as e:
        print(f"[Fehler beim Senden von DataRef {dataref_path} via XPC]: {e}")

def serial_to_xplane(ser, client):
    """Liest die Binärpakete vom Pico aus und leitet sie über XPC an X-Plane weiter."""
    global running
    print("[Bridge] Serial-to-XPlane Thread gestartet (via XPC).")
    
    while running:
        try:
            b1 = ser.read(1)
            if not b1:
                continue
            if b1 == b'\xab':
                b2 = ser.read(1)
                if b2 == b'\xcd':
                    rest_data = ser.read(struct.calcsize(PACKET_FORMAT) - 2)
                    if len(rest_data) == struct.calcsize(PACKET_FORMAT) - 2:
                        full_data = b'\xab\xcd' + rest_data
                
                        header_bytes, target_hdg, target_vs, target_alt, target_elev_trim, modes, LGearSt, checksum = struct.unpack(PACKET_FORMAT, full_data)

                        print(f"[Pico -> XP] HDG: {target_hdg}°, ALT: {target_alt}ft, VS: {target_vs}fpm, Trim: {target_elev_trim}%, Modes: {modes}")

                        # 1. Standard-Datarefs für die Zielwerte senden (sauber via XPC)
                        send_dataref_to_xplane(client, "sim/cockpit2/autopilot/heading_dial_deg_mag_pilot", target_hdg)
                        send_dataref_to_xplane(client, "sim/cockpit2/autopilot/altitude_dial_ft", target_alt)
                        send_dataref_to_xplane(client, "sim/cockpit2/autopilot/vvi_dial_fpm", target_vs)

                        trim_scaled = float(target_elev_trim) / 100.0
                        send_dataref_to_xplane(client, "sim/flightmodel/controls/elv_trim", trim_scaled)

                        gear_value = 0 if LGearSt else 1
                        send_dataref_to_xplane(client, "sim/cockpit2/controls/gear_handle_down", gear_value)

                        """# 2. Autopilot-Modes senden
                        hdg_engaged = 1 if (modes & (1 << 0)) else 0
                        alt_engaged = 1 if (modes & (1 << 1)) else 0
                        vs_engaged  = 1 if (modes & (1 << 2)) else 0
                        nav_engaged = 1 if (modes & (1 << 3)) else 0
                        ap_engaged  = 1 if (modes & (1 << 4)) else 0

                        send_dataref_to_xplane(client, "sim/cockpit2/autopilot/heading_status", hdg_engaged)
                        send_dataref_to_xplane(client, "sim/cockpit2/autopilot/altitude_hold_status", alt_engaged)
                        send_dataref_to_xplane(client, "sim/cockpit2/autopilot/vspeed_status", vs_engaged)
                        send_dataref_to_xplane(client, "sim/cockpit2/autopilot/nav_status", nav_engaged)
                        send_dataref_to_xplane(client, "sim/cockpit2/autopilot/autopilot_on", ap_engaged)"""
        
        except Exception as e:
            print(f"[Fehler Serial->XP]: {e}")
            time.sleep(0.1)

def xplane_to_serial(ser):
    """Lauscht auf Telemetriedaten von X-Plane, parst sie und schickt sie an den Pico."""
    global running
    print("[Bridge] XPlane-to-Serial Thread gestartet.")
    
    # Eigener Empfangs-Socket nur für Port 49001
    sock_recv = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock_recv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        sock_recv.bind(("0.0.0.0", XP_LISTEN_PORT))
        print(f"[Bridge] Erfolgreich auf UDP Port {XP_LISTEN_PORT} gebunden.")
    except Exception as e:
        print(f"[FEHLER] Konnte Port {XP_LISTEN_PORT} nicht binden (Bereits belegt?): {e}")
        return

    sock_recv.settimeout(1.0)

    flight_data = {
        "airspeed": 0.0, "altitude": 0.0, "vspeed": 0.0, "heading": 0.0,
        "throttle": 0.0, "mixture": 100.0, "rudder": 0.0,
        "gear_down": 1, "flaps_pos": 0, "ap_engaged": 0,
        "ap_heading": 0, "ap_vspeed": 0, "ap_altitude": 0,
        "elev_trim": 0, "ap_modes": 0
    }

    last_pico_send_time = 0

    while running:
        try:
            data, addr = sock_recv.recvfrom(2048)

            if data.startswith(b'DATA'):
                payload = data[5:]
                for i in range(0, len(payload), 36):
                    chunk = payload[i:i+36]
                    if len(chunk) == 36:
                        row_index, f1, f2, f3, f4, f5, f6, f7, f8 = struct.unpack('<i8f', chunk)
                        
                        if row_index == 3: 
                            flight_data["airspeed"] = f1
                        elif row_index == 17: 
                            flight_data["heading"] = f3
                        elif row_index == 20: 
                            # FIX 3: In Zeile 20 ist f1 oft Latitude, die echte Höhe liegt meist auf f3 (oder f4 je nach XP-Version)
                            flight_data["altitude"] = f3 
                        elif row_index == 26: 
                            flight_data["throttle"] = f1 * 100.0
                        elif row_index == 29: 
                            flight_data["mixture"] = f1 * 100.0
                        elif row_index == 11: 
                            flight_data["rudder"] = f3 * 100.0
                        elif row_index == 14: 
                            flight_data["gear_down"] = 1 if f1 > 0.5 else 0
                        elif row_index == 13:
                            # FIX 5: Klappen hängen meist auf f2 oder f3 statt f1
                            flight_data["flaps_pos"] = f4 * 100.0
                            flight_data["elev_trim"] = int(f1 * 100)
                        elif row_index == 4:
                            # FIX 4: Echte Flug-VSpeed (VVI) kommt aus Zeile 4, nicht aus dem Autopilot-Panel!
                            flight_data["vspeed"] = int(f3) 
                        elif row_index == 117:
                            hdg_mode_val = int(f2)
                            alt_mode_val = int(f3)
    
                            # 1. Einzelne Modi in Bits übersetzen (basierend auf deinen X-Plane Werten)
                            hdg_bit = 1 if hdg_mode_val == 1 else 0          # 1 = HDG aktiv
                            nav_bit = 1 if hdg_mode_val == 2 else 0          # 2 = NAV aktiv
    
                            alt_bit = 1 if alt_mode_val == 6 else 0          # 6 = ALT-Hold aktiv
                            vs_bit  = 1 if alt_mode_val == 4 else 0          # 4 = VS-Modus aktiv
    
                            # 2. Zu einer einzigen Bitmaske zusammenbauen (exakt wie dein Pico es shiftet)
                            ap_modes_mask = (hdg_bit << 0) | (alt_bit << 1) | (vs_bit << 2) | (nav_bit << 3)
    
                            # 3. Autopilot Master (Engaged): Ist an, solange nicht beides auf "OFF" steht (HDG=15, ALT=12)
                            ap_is_engaged = 0 if (hdg_mode_val == 15 and alt_mode_val == 12) else 1
    
                            # Ins flight-Data Dictionary schreiben, damit es an den Pico geht
                            flight_data["ap_modes"] = ap_modes_mask
                            flight_data["ap_engaged"] = ap_is_engaged
                        elif row_index == 118:
                            # Das ist das Autopilot-Zielpanel (Target-Werte)
                            flight_data["ap_heading"] = int(f2)
                            flight_data["ap_altitude"] = int(f4)
                            flight_data["ap_vspeed"] = int(f3)  # Das ist die eingestellte AP-VSpeed
                            ##flight_data["ap_engaged"] = 1 if f4 > 0.5 else 0
                
                # Nur alle 100 ms (10x pro Sekunde) an den Pico senden
                current_time = time.time()
                if current_time - last_pico_send_time >= 0.1:
                    print(f"Sende an Pico -> Airspeed: {flight_data['airspeed']:.1f} , AP: {flight_data['ap_engaged']:.1f}")
                    send_telemetry_to_pico(ser, flight_data)
                    last_pico_send_time = current_time

        except socket.timeout:
            continue
        except Exception as e:
            print(f"[Fehler XP->Serial]: {e}")
            
    sock_recv.close()

def send_telemetry_to_pico(ser, flight_data):
    """Verpackt die X-Plane Daten in das Telemetrie-Paket und sendet es an den Pico."""
    header = b'\xfe\xed'
    
    # Werte absichern, damit 'B' (0-255) niemals durch negative Zahlen crasht
    gear = max(0, min(255, int(flight_data['gear_down'])))
    flaps = max(0, min(255, int(flight_data['flaps_pos'])))
    ap_eng = max(0, min(255, int(flight_data['ap_engaged'])))
    ap_mod = max(0, min(255, int(flight_data['ap_modes'])))
    
    # Trim kann negativ sein (wird durch 'b' als signed char abgedeckt: -128 bis 127)
    trim = max(-128, min(127, int(flight_data['elev_trim'])))
    
    temp_packet = struct.pack(
        TELEMETRY_FORMAT,
        header,
        float(flight_data['airspeed']), float(flight_data['altitude']), float(flight_data['vspeed']),
        float(flight_data['heading']), float(flight_data['throttle']), float(flight_data['mixture']),
        float(flight_data['rudder']), gear, flaps,
        ap_eng, int(flight_data['ap_heading']), int(flight_data['ap_vspeed']),
        int(flight_data['ap_altitude']), trim, ap_mod,
        0
    )
    
    checksum = 0
    for b in temp_packet[2:-1]:
        checksum ^= b
        
    final_packet = struct.pack(
        TELEMETRY_FORMAT,
        header,
        float(flight_data['airspeed']), float(flight_data['altitude']), float(flight_data['vspeed']),
        float(flight_data['heading']), float(flight_data['throttle']), float(flight_data['mixture']),
        float(flight_data['rudder']), gear, flaps,
        ap_eng, int(flight_data['ap_heading']), int(flight_data['ap_vspeed']),
        int(flight_data['ap_altitude']), trim, ap_mod,
        checksum
    )
    
    try:
        ser.write(final_packet)
    except Exception as e:
        print(f"[Fehler beim Senden zum Pico]: {e}")

def main():
    global running
    print(f"Verbinde mit Pico auf {SERIAL_PORT}...")
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
    except Exception as e:
        print(f"Konnte seriellen Port nicht öffnen: {e}")
        return

    # XPC Client initialisieren (verbindet sich standardmäßig mit X-Plane auf localhost)
    try:
        print("Verbinde mit X-Plane über XPC...")
        client = xpc.XPlaneConnect()
        print("XPC Verbindung erfolgreich hergestellt!")
    except Exception as e:
        print(f"[FEHLER] Konnte keine Verbindung zu XPC herstellen: {e}")
        ser.close()
        return

    # Threads starten (übergebe den xpc-client statt sock_send)
    t_s2x = threading.Thread(target=serial_to_xplane, args=(ser, client))
    t_x2s = threading.Thread(target=xplane_to_serial, args=(ser,))

    t_s2x.daemon = True
    t_x2s.daemon = True

    t_s2x.start()
    t_x2s.start()

    print("Brücke läuft. Drücke Strg+C zum Beenden.")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Beende Brücke...")
        running = False
        t_s2x.join()
        t_x2s.join()
        ser.close()

if __name__ == "__main__":
    main()