# TemplateServerCPP

**Para eliminar completamente la vinculación con el repositorio remoto:**

```bash
git clone https://github.com/JesuDvS/TemplateServerCPP.git .
git remote remove origin
git remote -v
rm -rf .git
git init
git add .
git commit -m "Initial commit"
```

```bash
git remote add origin @@@@
git branch -M main
git push -u origin main
```
**Para actualizar crow_all.h usa lo siguiente en un futuro:**

```bash
wget https://github.com/CrowCpp/Crow/releases/download/v1.0+5/crow_all.h -O include/crow_all.h
```

**Dale permiso para compilar automaticamente para las dos arquitecturas**

```bash
chmod +x rebuild-system.sh
```

**Ejecutar**

```bash👎 😕 
./rebuild-system.sh
```

Compilar aun cuando salga error en el main.cpp

cambiar el nombre del ejecutable en el CMakeList.txt

```c
set(EXECUTABLE_NAME auto_sync_server)
```

**Para una compilacion cruzada para arm desde x86-64**

Se nececita ajustes para habilitar libreria de arm dentro de x86-64, se agregara mas adelante esa configuracion aqui ....😕
