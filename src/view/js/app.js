// Estado global
let ws = null;
let reconnectInterval = null;
let myIP = null;

// Elementos DOM
const chatContainer = document.getElementById('chatContainer');
const messageInput = document.getElementById('messageInput');
const sendBtn = document.getElementById('sendBtn');
const fileInput = document.getElementById('fileInput');
const fileUploadBtn = document.getElementById('fileUploadBtn');
const statusDot = document.getElementById('statusDot');
const statusText = document.getElementById('statusText');
const dropZone = document.getElementById('dropZone');

// 🔥 NUEVO: Auto-resize del textarea
function autoResizeTextarea() {
    messageInput.style.height = 'auto';
    const newHeight = Math.min(messageInput.scrollHeight, 120);
    messageInput.style.height = newHeight + 'px';
}

// Obtener IP del cliente
async function getMyIP() {
    try {
        const response = await fetch('/api/my_ip');
        if (!response.ok) {
            console.warn('⚠️ No se pudo obtener IP, usando comparación por timestamp');
            return null;
        }
        const data = await response.json();
        myIP = data.ip;
        console.log('📍 Mi IP:', myIP);
        return myIP;
    } catch (error) {
        console.error('❌ Error obteniendo IP:', error);
        return null;
    }
}

// Conectar WebSocket
function connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    
    console.log('🔌 Conectando a:', wsUrl);
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
        try {
            const data = JSON.parse(event.data);
            console.log('📨 Mensaje recibido:', data);
            
            if (data.type === 'initial_state') {
                console.log(`📦 Cargando ${data.messages.length} mensajes iniciales`);
                chatContainer.innerHTML = '';
                
                data.messages.forEach((msg, index) => {
                    console.log(`  ${index + 1}. Mensaje:`, msg);
                    const isMine = isMyMessage(msg);
                    addMessageToUI(msg, isMine);
                });
                
                scrollToBottom();
                console.log('✅ Mensajes cargados');
                
            } else if (data.type === 'new_message') {
                console.log('🆕 Nuevo mensaje:', data.message);
                const isMine = isMyMessage(data.message);
                addMessageToUI(data.message, isMine);
                scrollToBottom();
            }
        } catch (error) {
            console.error('❌ Error procesando mensaje WebSocket:', error);
            console.error('   Datos recibidos:', event.data);
        }
    };
    
    ws.onerror = (error) => {
        console.error('❌ Error WebSocket:', error);
    };
    
    ws.onclose = () => {
        console.log('🔌 WebSocket desconectado');
        statusDot.classList.add('disconnected');
        statusText.textContent = 'Desconectado';
        
        if (!reconnectInterval) {
            reconnectInterval = setInterval(() => {
                console.log('🔄 Intentando reconectar...');
                connectWebSocket();
            }, 3000);
        }
    };
}

// Determinar si un mensaje es mío
function isMyMessage(message) {
    if (!myIP || !message.sender_ip) {
        console.log('⚠️ No hay IP para comparar, asumiendo mensaje externo');
        return false;
    }
    
    const senderIP = message.sender_ip.split(':')[0];
    const result = senderIP === myIP || message.sender_ip.includes(myIP);
    
    console.log(`🔍 ¿Es mi mensaje? ${result} (sender: ${message.sender_ip}, myIP: ${myIP})`);
    return result;
}

// Copiar texto al portapapeles
async function copyToClipboard(text, buttonElement) {
    try {
        await navigator.clipboard.writeText(text);
        
        const originalContent = buttonElement.innerHTML;
        buttonElement.classList.add('copied');
        buttonElement.innerHTML = '';
        
        setTimeout(() => {
            buttonElement.classList.remove('copied');
            buttonElement.innerHTML = originalContent;
        }, 2000);
        
        console.log('✅ Texto copiado');
    } catch (error) {
        console.error('❌ Error al copiar:', error);
        
        const textarea = document.createElement('textarea');
        textarea.value = text;
        textarea.style.position = 'fixed';
        textarea.style.opacity = '0';
        document.body.appendChild(textarea);
        textarea.select();
        
        try {
            document.execCommand('copy');
            
            const originalContent = buttonElement.innerHTML;
            buttonElement.classList.add('copied');
            buttonElement.innerHTML = '';
            
            setTimeout(() => {
                buttonElement.classList.remove('copied');
                buttonElement.innerHTML = originalContent;
            }, 2000);
            
            console.log('✅ Texto copiado (fallback)');
        } catch (err) {
            console.error('❌ Error en fallback:', err);
            alert('No se pudo copiar el texto');
        }
        
        document.body.removeChild(textarea);
    }
}

// Detectar y formatear código
function formatMessageContent(text) {
    const codeBlockRegex = /```([\s\S]*?)```/g;
    let formatted = escapeHtml(text);
    
    formatted = formatted.replace(codeBlockRegex, (match, code) => {
        return `<pre><code>${code.trim()}</code></pre>`;
    });
    
    const inlineCodeRegex = /`([^`]+)`/g;
    formatted = formatted.replace(inlineCodeRegex, (match, code) => {
        if (!match.includes('<pre>')) {
            return `<code>${code}</code>`;
        }
        return match;
    });
    
    return formatted;
}

// Manejar expansión de mensajes largos
function handleMessageExpansion(contentDiv, text) {
    const lineCount = text.split('\n').length;
    const charCount = text.length;
    
    if (lineCount > 10 || charCount > 500) {
        contentDiv.classList.add('collapsed');
        
        const expandBtn = document.createElement('button');
        expandBtn.className = 'expand-btn';
        expandBtn.textContent = '▼ Ver más';
        
        expandBtn.addEventListener('click', () => {
            if (contentDiv.classList.contains('collapsed')) {
                contentDiv.classList.remove('collapsed');
                contentDiv.classList.add('expanded');
                expandBtn.textContent = '▲ Ver menos';
            } else {
                contentDiv.classList.remove('expanded');
                contentDiv.classList.add('collapsed');
                expandBtn.textContent = '▼ Ver más';
                
                contentDiv.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
            }
        });
        
        return expandBtn;
    }
    
    return null;
}

// Agregar mensaje al UI
function addMessageToUI(message, isSent) {
    try {
        console.log(`🎨 Renderizando mensaje:`, {
            id: message.id,
            type: message.type,
            isSent,
            sender: message.sender_ip,
            content: message.content?.substring(0, 50) + '...'
        });
        
        const messageDiv = document.createElement('div');
        messageDiv.className = `message ${isSent ? 'sent' : 'received'}`;
        messageDiv.setAttribute('data-message-id', message.id);
        
        const wrapper = document.createElement('div');
        wrapper.className = 'message-wrapper';
        
        const bubble = document.createElement('div');
        bubble.className = 'message-bubble';
        
        if (message.type === 'file') {
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
            const content = document.createElement('div');
            content.className = 'message-content';
            content.innerHTML = formatMessageContent(message.content);
            
            bubble.appendChild(content);
            
            const expandBtn = handleMessageExpansion(content, message.content);
            if (expandBtn) {
                bubble.appendChild(expandBtn);
            }
            
            const copyBtn = document.createElement('button');
            copyBtn.className = 'copy-btn';
            copyBtn.innerHTML = '📋';
            copyBtn.title = 'Copiar mensaje';
            copyBtn.setAttribute('aria-label', 'Copiar mensaje al portapapeles');
            
            copyBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                copyToClipboard(message.content, copyBtn);
            });
            
            wrapper.appendChild(copyBtn);
        }
        
        const meta = document.createElement('div');
        meta.className = 'message-meta';
        
        const sender = document.createElement('span');
        sender.className = 'message-sender';
        sender.textContent = isSent ? 'Tú' : (message.sender_ip ? message.sender_ip.split(':')[0] : 'Desconocido');
        
        const time = document.createElement('span');
        time.className = 'message-time';
        time.textContent = formatTime(message.timestamp);
        
        meta.appendChild(sender);
        meta.appendChild(time);
        bubble.appendChild(meta);
        
        wrapper.appendChild(bubble);
        messageDiv.appendChild(wrapper);
        chatContainer.appendChild(messageDiv);
        
        console.log('✅ Mensaje renderizado en DOM:', message.id);
        
    } catch (error) {
        console.error('❌ Error renderizando mensaje:', error);
        console.error('   Mensaje que causó el error:', message);
    }
}

// Enviar mensaje de texto
async function sendTextMessage() {
    const text = messageInput.value.trim();
    if (!text) return;
    
    console.log('📤 Enviando mensaje:', text.substring(0, 50) + '...');
    
    try {
        const response = await fetch('/api/send_text', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ text })
        });
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        const data = await response.json();
        if (data.success) {
            messageInput.value = '';
            autoResizeTextarea();
            console.log('✅ Mensaje enviado:', data.message_id);
        } else {
            console.error('❌ El servidor rechazó el mensaje:', data);
        }
    } catch (error) {
        console.error('❌ Error al enviar mensaje:', error);
        alert('Error al enviar el mensaje: ' + error.message);
    }
}

// Subir archivos
async function uploadFiles(files) {
    for (const file of files) {
        console.log('📤 Subiendo archivo:', file.name);
        
        const formData = new FormData();
        formData.append('file', file);
        
        try {
            const response = await fetch('/api/upload', {
                method: 'POST',
                body: formData
            });
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            const data = await response.json();
            if (data.success) {
                console.log('✅ Archivo subido:', data.filename);
            }
        } catch (error) {
            console.error('❌ Error al subir archivo:', error);
            alert(`Error al subir ${file.name}: ${error.message}`);
        }
    }
}

// Descargar archivo con progreso
async function downloadFile(filename, originalName) {
    const downloadBtn = event.target;
    downloadBtn.classList.add('downloading');
    downloadBtn.disabled = true;
    
    try {
        const response = await fetch(`/api/download/${filename}`);
        
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        const contentLength = response.headers.get('Content-Length');
        const total = parseInt(contentLength, 10);
        
        const reader = response.body.getReader();
        const chunks = [];
        let receivedLength = 0;
        
        while (true) {
            const { done, value } = await reader.read();
            
            if (done) break;
            
            chunks.push(value);
            receivedLength += value.length;
            
            if (total) {
                const percent = Math.round((receivedLength / total) * 100);
                downloadBtn.textContent = `${percent}%`;
            }
        }
        
        const blob = new Blob(chunks);
        
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = originalName;
        document.body.appendChild(a);
        a.click();
        window.URL.revokeObjectURL(url);
        document.body.removeChild(a);
        
        console.log('✅ Archivo descargado:', originalName);
        
        downloadBtn.textContent = '⬇️ Descargar';
        
    } catch (error) {
        console.error('❌ Error al descargar:', error);
        alert('Error al descargar el archivo. Por favor intenta de nuevo.');
        downloadBtn.textContent = '❌ Error';
        
        setTimeout(() => {
            downloadBtn.textContent = '⬇️ Descargar';
        }, 2000);
    } finally {
        downloadBtn.classList.remove('downloading');
        downloadBtn.disabled = false;
    }
}

// Utilidades
function formatTime(timestamp) {
    try {
        const date = new Date(timestamp);
        if (isNaN(date.getTime())) {
            return timestamp;
        }
        return date.toLocaleTimeString('es-ES', { hour: '2-digit', minute: '2-digit' });
    } catch (error) {
        console.error('Error formateando tiempo:', error);
        return timestamp;
    }
}

function formatFileSize(bytes) {
    if (!bytes || bytes === 0) return '0 B';
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
}

function getFileIcon(filename) {
    if (!filename) return '📎';
    
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
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function scrollToBottom() {
    chatContainer.scrollTop = chatContainer.scrollHeight;
}

// Event Listeners
sendBtn.addEventListener('click', sendTextMessage);

// 🔥 CAMBIADO: Ctrl+Enter para enviar, Enter para nueva línea
messageInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && e.ctrlKey) {
        e.preventDefault();
        sendTextMessage();
    }
});

// 🔥 NUEVO: Auto-resize al escribir
messageInput.addEventListener('input', autoResizeTextarea);

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
let dragCounter = 0;

document.body.addEventListener('dragenter', (e) => {
    e.preventDefault();
    dragCounter++;
    if (dragCounter === 1) {
        dropZone.classList.remove('hidden');
        dropZone.style.display = 'flex';
    }
});

document.body.addEventListener('dragleave', (e) => {
    e.preventDefault();
    dragCounter--;
    if (dragCounter === 0) {
        dropZone.classList.add('hidden');
        dropZone.style.display = 'none';
    }
});

document.body.addEventListener('dragover', (e) => {
    e.preventDefault();
});

dropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    dragCounter = 0;
    dropZone.classList.add('hidden');
    dropZone.style.display = 'none';
    
    const files = Array.from(e.dataTransfer.files);
    if (files.length > 0) {
        uploadFiles(files);
    }
});

document.addEventListener('dragover', (e) => {
    e.preventDefault();
});

document.addEventListener('drop', (e) => {
    e.preventDefault();
});

// Inicialización
document.addEventListener('DOMContentLoaded', async () => {
    console.log('='.repeat(50));
    console.log('🚀 AutoSync Client iniciado');
    console.log('='.repeat(50));
    
    console.log('✓ chatContainer:', chatContainer ? 'OK' : '❌ NO ENCONTRADO');
    console.log('✓ messageInput:', messageInput ? 'OK' : '❌ NO ENCONTRADO');
    console.log('✓ sendBtn:', sendBtn ? 'OK' : '❌ NO ENCONTRADO');
    
    console.log('📍 Obteniendo mi IP...');
    await getMyIP();
    
    console.log('🔌 Conectando WebSocket...');
    connectWebSocket();
    
    // Configurar viewport height
    const setVH = () => {
        const vh = window.innerHeight * 0.01;
        document.documentElement.style.setProperty('--vh', `${vh}px`);
    };
    
    setVH();
    window.addEventListener('resize', setVH);
    window.addEventListener('orientationchange', setVH);
    
    // Inicializar altura del textarea
    autoResizeTextarea();
    
    console.log('='.repeat(50));
    console.log('✅ Inicialización completa');
    console.log('💡 Usa Ctrl+Enter para enviar mensajes');
    console.log('='.repeat(50));
});

// Exportar función global para descargas
window.downloadFile = downloadFile;