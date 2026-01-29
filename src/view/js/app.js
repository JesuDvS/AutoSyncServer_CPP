document.addEventListener('DOMContentLoaded', () => {
    const syncBtn = document.getElementById('syncBtn');
    const statusDiv = document.getElementById('status');

    syncBtn.addEventListener('click', async () => {
        try {
            statusDiv.textContent = 'Sincronizando...';
            
            const response = await fetch('/api/status');
            const data = await response.json();
            
            statusDiv.textContent = `✅ ${data.message} (${data.resources_loaded} recursos)`;
        } catch(error) {
            statusDiv.textContent = '❌ Error al conectar con el servidor';
        }
    });

    // Cargar recursos disponibles
    loadResources();
});

async function loadResources() {
    try {
        const response = await fetch('/api/resources');
        const data = await response.json();
        console.log('📦 Recursos embebidos:', data.paths);
    } catch(error) {
        console.error('Error cargando recursos:', error);
    }
}
