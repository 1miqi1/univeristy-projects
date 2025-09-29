# Rozmiar pliku docelowego w bajtach (4 GB)
target_size = 4 * 1024 * 1024 * 1024  # 4 GB

# Nazwa pliku do wygenerowania
file_name = "plik_4GB.bin"

# Rozmiar pojedynczego bloku danych (np. 1 MB)
block_size = 1024 * 1024  # 1 MB

# Dane do zapisania (np. same zera)
data_block = b'\x00' * block_size

with open(file_name, "wb") as f:
    written = 0
    while written < target_size:
        f.write(data_block)
        written += block_size
        print(f"Zapisano {written / (1024 * 1024):.2f} MB", end='\r')

print("\nPlik 4 GB został wygenerowany.")
