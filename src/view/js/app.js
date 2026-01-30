// Estado global
let ws = null;
let reconnectInterval = null;
let isMyMessage = false;

// Elementos DOM
const chatContainer = document.getElementById('chatContainer');
const messageInput = document.getElementById('messageInput');
const sendBtn = document.getElementById('sendBtn');
const fileInput = document.getElementById('fileInput');
const fileUploadBtn = document.getElementById('fileUploadBtn');
const statusDot = document.getElementById('statusDot');
const statusText = document.getElementById('statusText');
const dropZone = document.getElementById('dropZone');

// Conectar WebSocket
function connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    
    ws = new WebSocket(wsUrl);
    
    ws.onopen = () => {
        console.log('✅ WebSocket conectado');
        statusDot.classList.remove('disconnected');
        statusText.textContent = 'Conectado';
        
        if (reconnectInterval) {
            clearInterval(reconnectInterval);
            reconnectInterval = null;
        }
    };
    
    ws.onmessage = (event) => {
        const data = JSON.parse(event.data);
        console.log('📨 Mensaje recibido:', data);
        
        if (data.type === 'initial_state') {
            // Cargar estado inicial
            chatContainer.innerHTML = '';
            data.messages.forEach(msg => addMessageToUI(msg, false));
            scrollToBottom();
        } else if (data.type === 'new_message') {
            // Nuevo mensaje de otro cliente
            addMessageToUI(data.message, false);
            scrollToBottom();
        }
    };
    
    ws.onerror = (error) => {
        console.error('❌ Error WebSocket:', error);
    };
    
    ws.onclose = () => {
        console.log('🔌 WebSocket desconectado');
        statusDot.classList.add('disconnected');
        statusText.textContent = 'Desconectado';
        
        // Reconectar automáticamente
        if (!reconnectInterval) {
            reconnectInterval = setInterval(() => {
                console.log('🔄 Intentando reconectar...');
                connectWebSocket();
            }, 3000);
        }
    };
}

// Agregar mensaje al UI
function addMessageToUI(message, isSent) {
    const messageDiv = document.createElement('div');
    messageDiv.className = `message ${isSent ? 'sent' : 'received'}`;
    
    const bubble = document.createElement('div');
    bubble.className = 'message-bubble';
    
    if (message.type === 'file') {
        // Mensaje de archivo
        const fileAttachment = document.createElement('div');
        fileAttachment.className = 'file-attachment';
        
        const fileIcon = getFileIcon(message.content);
        
        fileAttachment.innerHTML = `
            <div class="file-icon">${fileIcon}</div>
            <div class="file-info">
                <div class="file-name">${escapeHtml(message.content)}</div>
                <div class="file-size">${formatFileSize(message.filesize)}</div>
            </div>
            <button class="download-btn" onclick="downloadFile('${message.filename}', '${escapeHtml(message.content)}')">
                ⬇️ Descargar
            </button>
        `;
        
        bubble.appendChild(fileAttachment);
    } else {
        // Mensaje de texto
        const content = document.createElement('div');
        content.className = 'message-content';
        content.textContent = message.content;
        bubble.appendChild(content);
    }
    
    // Metadatos
    const meta = document.createElement('div');
    meta.className = 'message-meta';
    
    const sender = document.createElement('span');
    sender.className = 'message-sender';
    sender.textContent = isSent ? 'Tú' : message.sender_ip.split(':')[0];
    
    const time = document.createElement('span');
    time.className = 'message-time';
    time.textContent = formatTime(message.timestamp);
    
    meta.appendChild(sender);
    meta.appendChild(time);
    bubble.appendChild(meta);
    
    messageDiv.appendChild(bubble);
    chatContainer.appendChild(messageDiv);
}

// Enviar mensaje de texto
async function sendTextMessage() {
    const text = messageInput.value.trim();
    if (!text) return;
    
    try {
        const response = await fetch('/api/send_text', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ text })
        });
        
        const data = await response.json();
        if (data.success) {
            messageInput.value = '';
            console.log('✅ Mensaje enviado:', data.message_id);
        }
    } catch (error) {
        console.error('❌ Error al enviar mensaje:', error);
        alert('Error al enviar el mensaje');
    }
}

// Subir archivos
async function uploadFiles(files) {
    for (const file of files) {
        const formData = new FormData();
        formData.append('file', file);
        
        try {
            const response = await fetch('/api/upload', {
                method: 'POST',
                body: formData
            });
            
            const data = await response.json();
            if (data.success) {
                console.log('✅ Archivo subido:', data.filename);
            }
        } catch (error) {
            console.error('❌ Error al subir archivo:', error);
            alert(`Error al subir ${file.name}`);
        }
    }
}

// Descargar archivo
async function downloadFile(filename, originalName) {
    try {
        const response = await fetch(`/api/download/${filename}`);
        const blob = await response.blob();
        
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = originalName;
        document.body.appendChild(a);
        a.click();
        window.URL.revokeObjectURL(url);
        document.body.removeChild(a);
        
        console.log('✅ Archivo descargado:', originalName);
    } catch (error) {
        console.error('❌ Error al descargar:', error);
        alert('Error al descargar el archivo');
    }
}

// Utilidades
function formatTime(timestamp) {
    const date = new Date(timestamp);
    return date.toLocaleTimeString('es-ES', { hour: '2-digit', minute: '2-digit' });
}

function formatFileSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
}

function getFileIcon(filename) {
    const ext = filename.split('.').pop().toLowerCase();
    
    const icons = {
        pdf: '📄', doc: '📝', docx: '📝', txt: '📝',
        jpg: '🖼️', jpeg: '🖼️', png: '🖼️', gif: '🖼️', svg: '🖼️',
        mp4: '🎥', avi: '🎥', mov: '🎥',
        mp3: '🎵', wav: '🎵', flac: '🎵',
        zip: '📦', rar: '📦', '7z': '📦',
        xls: '📊', xlsx: '📊', csv: '📊'
    };
    
    return icons[ext] || '📎';
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function scrollToBottom() {
    chatContainer.scrollTop = chatContainer.scrollHeight;
}

// Event Listeners
sendBtn.addEventListener('click', sendTextMessage);

messageInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        sendTextMessage();
    }
});

fileUploadBtn.addEventListener('click', () => {
    fileInput.click();
});

fileInput.addEventListener('change', (e) => {
    if (e.target.files.length > 0) {
        uploadFiles(Array.from(e.target.files));
        fileInput.value = '';
    }
});

// Drag and Drop
document.body.addEventListener('dragover', (e) => {
    e.preventDefault();
    dropZone.classList.remove('hidden');
});

dropZone.addEventListener('dragleave', (e) => {
    if (e.target === dropZone) {
        dropZone.classList.add('hidden');
    }
});

dropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropZone.classList.add('hidden');
    
    const files = Array.from(e.dataTransfer.files);
    if (files.length > 0) {
        uploadFiles(files);
    }
});

// Inicializar
document.addEventListener('DOMContentLoaded', () => {
    console.log('🚀 AutoSync Client iniciado');
    connectWebSocket();
});

// Exponer función global para descargas
window.downloadFile = downloadFile;