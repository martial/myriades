#!/usr/bin/env python3
"""
Interactive OSC command sender for testing HourGlass LED commands

Usage: 
  python test_osc_commands.py           # Interactive mode (default)
  python test_osc_commands.py auto      # Automated test sequence

Interactive commands:
  rgb 1 255 0 0      # Set RGB red on hourglass 1
  rgb all 0 255 0    # Set RGB green on all hourglasses
  up 1 255 255 0     # Set UP LED yellow on hourglass 1
  down 1 255 0 255   # Set DOWN LED magenta on hourglass 1
  blend 1 384        # Set blend to middle circle
  origin 1 90        # Set origin to 90 degrees
  arc 1 180          # Set arc to 180 degrees (half circle)
  lum 1 0.5          # Set individual luminosity to 50%
  global 0.8         # Set global luminosity to 80%
  blackout           # Turn off all LEDs
  demo               # Run quick demo
  help               # Show help
  quit               # Exit
"""

import time
from pythonosc import udp_client

class OSCTester:
    def __init__(self, host="127.0.0.1", port=8000):
        """Initialize OSC client"""
        self.client = udp_client.SimpleUDPClient(host, port)
        print(f"OSC Client initialized - sending to {host}:{port}")
    
    def send_led_all_rgb(self, hourglass_id, r, g, b):
        """Send RGB command to all LEDs"""
        address = f"/hourglass/{hourglass_id}/led/all/rgb"
        self.client.send_message(address, [r, g, b])
        print(f"Sent: {address} {r} {g} {b}")
    
    def send_led_all_blend(self, hourglass_id, blend):
        """Send blend command to all LEDs (0-768)"""
        address = f"/hourglass/{hourglass_id}/led/all/blend"
        self.client.send_message(address, [blend])
        print(f"Sent: {address} {blend}")
    
    def send_led_all_origin(self, hourglass_id, origin):
        """Send origin command to all LEDs (0-360)"""
        address = f"/hourglass/{hourglass_id}/led/all/origin"
        self.client.send_message(address, [origin])
        print(f"Sent: {address} {origin}")
    
    def send_led_all_arc(self, hourglass_id, arc):
        """Send arc command to all LEDs (0-360)"""
        address = f"/hourglass/{hourglass_id}/led/all/arc"
        self.client.send_message(address, [arc])
        print(f"Sent: {address} {arc}")
    
    def send_individual_led(self, hourglass_id, side, command, *args):
        """Send command to individual LED (up/down)"""
        address = f"/hourglass/{hourglass_id}/{side}/{command}"
        self.client.send_message(address, list(args))
        print(f"Sent: {address} {list(args)}")
    
    def send_luminosity(self, hourglass_id, luminosity):
        """Send individual luminosity (0.0-1.0)"""
        address = f"/hourglass/{hourglass_id}/luminosity"
        self.client.send_message(address, [luminosity])
        print(f"Sent: {address} {luminosity}")
    
    def send_global_luminosity(self, luminosity):
        """Send global luminosity (0.0-1.0)"""
        address = "/system/luminosity"
        self.client.send_message(address, [luminosity])
        print(f"Sent: {address} {luminosity}")
    
    def send_blackout(self):
        """Send global blackout"""
        address = "/blackout"
        self.client.send_message(address, [])
        print(f"Sent: {address}")

def main():
    # Initialize OSC client
    osc = OSCTester()
    
    print("\n=== Testing OSC Commands ===\n")
    
    # Test 1: RGB All Commands
    print("1. Testing RGB ALL commands:")
    osc.send_led_all_rgb(1, 255, 0, 0)    # Red
    time.sleep(1)
    osc.send_led_all_rgb(1, 0, 255, 0)    # Green  
    time.sleep(1)
    osc.send_led_all_rgb(1, 0, 0, 255)    # Blue
    time.sleep(1)
    
    # Test 2: LED Effects
    print("\n2. Testing LED effects:")
    osc.send_led_all_blend(1, 384)        # Middle circle
    time.sleep(0.5)
    osc.send_led_all_origin(1, 90)        # Start at 90°
    time.sleep(0.5)
    osc.send_led_all_arc(1, 180)          # Half circle
    time.sleep(1)
    
    # Test 3: Individual LEDs
    print("\n3. Testing individual LEDs:")
    osc.send_individual_led(1, "up", "rgb", 255, 255, 0)    # Yellow up
    time.sleep(0.5)
    osc.send_individual_led(1, "down", "rgb", 255, 0, 255)  # Magenta down
    time.sleep(1)
    
    # Test 4: Luminosity
    print("\n4. Testing luminosity:")
    osc.send_luminosity(1, 0.5)           # 50% individual
    time.sleep(0.5)
    osc.send_global_luminosity(0.7)       # 70% global
    time.sleep(1)
    
    # Test 5: Multiple hourglasses
    print("\n5. Testing multiple hourglasses:")
    osc.send_led_all_rgb("all", 128, 128, 128)  # Gray all
    time.sleep(1)
    osc.send_led_all_rgb("1-3", 255, 128, 0)   # Orange range
    time.sleep(1)
    
    # Test 6: Reset
    print("\n6. Reset to defaults:")
    osc.send_blackout()                    # Blackout
    time.sleep(0.5)
    osc.send_global_luminosity(1.0)       # Full brightness
    osc.send_led_all_rgb(1, 255, 255, 255)  # White
    osc.send_led_all_blend(1, 0)          # Inner circle
    osc.send_led_all_origin(1, 0)         # Origin 0°
    osc.send_led_all_arc(1, 360)          # Full circle
    
    print("\n=== Test Complete ===")

def interactive_mode():
    """Interactive mode for manual testing"""
    osc = OSCTester()
    
    print("\n=== Interactive OSC Tester ===")
    print("Connected to OSC server on 127.0.0.1:8000")
    print("\nCommands:")
    print("  rgb <hg> <r> <g> <b>     - Set RGB (e.g: rgb 1 255 0 0, rgb all 255 0 0)")
    print("  up <hg> <r> <g> <b>      - Set UP LED RGB (e.g: up 1 255 0 0)")
    print("  down <hg> <r> <g> <b>    - Set DOWN LED RGB (e.g: down 1 0 255 0)")
    print("  blend <hg> <value>       - Set blend 0-768 (e.g: blend 1 384)")
    print("  origin <hg> <degrees>    - Set origin 0-360 (e.g: origin 1 90)")
    print("  arc <hg> <degrees>       - Set arc 0-360 (e.g: arc 1 180)")
    print("  lum <hg> <value>         - Set individual luminosity 0-1 (e.g: lum 1 0.5)")
    print("  global <value>           - Set global luminosity 0-1 (e.g: global 0.8)")
    print("  blackout                 - Global blackout")
    print("  help                     - Show this help")
    print("  demo                     - Run quick demo")
    print("  quit                     - Exit")
    print("\nHourglass ID can be: 1, 2, 3, all, 1-3, etc.")
    print()
    
    while True:
        try:
            cmd = input("OSC> ").strip().split()
            if not cmd:
                continue
                
            if cmd[0] == "quit" or cmd[0] == "q":
                break
            elif cmd[0] == "help" or cmd[0] == "h":
                print("\nCommands:")
                print("  rgb <hg> <r> <g> <b>     - Set RGB (e.g: rgb 1 255 0 0, rgb all 255 0 0)")
                print("  up <hg> <r> <g> <b>      - Set UP LED RGB (e.g: up 1 255 0 0)")
                print("  down <hg> <r> <g> <b>    - Set DOWN LED RGB (e.g: down 1 0 255 0)")
                print("  blend <hg> <value>       - Set blend 0-768 (e.g: blend 1 384)")
                print("  origin <hg> <degrees>    - Set origin 0-360 (e.g: origin 1 90)")
                print("  arc <hg> <degrees>       - Set arc 0-360 (e.g: arc 1 180)")
                print("  lum <hg> <value>         - Set individual luminosity 0-1 (e.g: lum 1 0.5)")
                print("  global <value>           - Set global luminosity 0-1 (e.g: global 0.8)")
                print("  blackout                 - Global blackout")
                print("  demo                     - Run quick demo")
                print("  quit                     - Exit")
            elif cmd[0] == "demo":
                print("Running demo...")
                osc.send_led_all_rgb(1, 255, 0, 0)
                time.sleep(0.5)
                osc.send_led_all_rgb(1, 0, 255, 0)
                time.sleep(0.5)
                osc.send_led_all_rgb(1, 0, 0, 255)
                time.sleep(0.5)
                osc.send_led_all_rgb(1, 255, 255, 255)
                print("Demo complete!")
            elif cmd[0] == "rgb" and len(cmd) == 5:
                osc.send_led_all_rgb(cmd[1], int(cmd[2]), int(cmd[3]), int(cmd[4]))
            elif cmd[0] == "up" and len(cmd) == 5:
                osc.send_individual_led(cmd[1], "up", "rgb", int(cmd[2]), int(cmd[3]), int(cmd[4]))
            elif cmd[0] == "down" and len(cmd) == 5:
                osc.send_individual_led(cmd[1], "down", "rgb", int(cmd[2]), int(cmd[3]), int(cmd[4]))
            elif cmd[0] == "blend" and len(cmd) == 3:
                osc.send_led_all_blend(cmd[1], int(cmd[2]))
            elif cmd[0] == "origin" and len(cmd) == 3:
                osc.send_led_all_origin(cmd[1], int(cmd[2]))
            elif cmd[0] == "arc" and len(cmd) == 3:
                osc.send_led_all_arc(cmd[1], int(cmd[2]))
            elif cmd[0] == "lum" and len(cmd) == 3:
                osc.send_luminosity(cmd[1], float(cmd[2]))
            elif cmd[0] == "global" and len(cmd) == 2:
                osc.send_global_luminosity(float(cmd[2]))
            elif cmd[0] == "blackout":
                osc.send_blackout()
            else:
                print("Invalid command or wrong number of arguments. Type 'help' for usage.")
        except KeyboardInterrupt:
            break
        except ValueError as e:
            print(f"Invalid number format: {e}")
        except Exception as e:
            print(f"Error: {e}")
    
    print("Goodbye!")

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1 and sys.argv[1] == "auto":
        main()
    else:
        # Run interactive mode by default
        interactive_mode()
