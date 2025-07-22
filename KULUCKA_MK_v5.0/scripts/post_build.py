Import("env")

def after_build(source, target, env):
    print("\n=== Derleme Tamamlandı ===")
    print("Hedef Platform: ESP32")
    print("FRAM Boyutu: 32KB")
    print("I2C Adresleri:")
    print("  - SHT31 Sensör 1: 0x44")
    print("  - SHT31 Sensör 2: 0x45")
    print("  - FRAM (MB85RC256V): 0x50")
    print("  - RTC (DS3231): 0x68")
    print("========================\n")

env.AddPostAction("buildprog", after_build)