Import("env")

def before_build(source, target, env):
    print("FRAM Modülü Aktif - 32KB hızlı bellek kullanılacak")
    print("I2C Bus Yönetimi Aktif - Thread-safe erişim sağlanacak")
    
    # Build flag kontrolü
    build_flags = env.get("BUILD_FLAGS", [])
    if "-DUSE_FRAM=1" in build_flags:
        print("✓ FRAM desteği etkin")
    else:
        print("✗ UYARI: FRAM desteği etkin değil!")

env.AddPreAction("buildprog", before_build)