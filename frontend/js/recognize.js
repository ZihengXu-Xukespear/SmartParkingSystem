// Recognize page logic
const user = checkAuth();
if (user) initSidebar();

let mediaStream = null;
let lastRecognizedPlate = null;

// ========== Camera Management ==========
async function startCamera() {
    const video = document.getElementById('camera-preview');
    const placeholder = document.getElementById('camera-placeholder');
    const btnStart = document.getElementById('btn-start-camera');
    const btnCapture = document.getElementById('btn-capture');
    const btnStop = document.getElementById('btn-stop-camera');

    try {
        mediaStream = await navigator.mediaDevices.getUserMedia({
            video: { width: { ideal: 1280 }, height: { ideal: 720 }, facingMode: 'environment' }
        });
        video.srcObject = mediaStream;
        video.style.display = 'block';
        placeholder.style.display = 'none';
        btnStart.disabled = true;
        btnCapture.disabled = false;
        btnStop.disabled = false;
    } catch (e) {
        if (e.name === 'NotAllowedError') {
            showError('result-alert', '摄像头权限被拒绝，请在浏览器设置中允许摄像头访问');
        } else if (e.name === 'NotFoundError') {
            showError('result-alert', '未检测到摄像头设备');
        } else {
            showError('result-alert', '摄像头启动失败: ' + e.message);
        }
    }
}

function stopCamera() {
    if (mediaStream) {
        mediaStream.getTracks().forEach(track => track.stop());
        mediaStream = null;
    }
    const video = document.getElementById('camera-preview');
    video.style.display = 'none';
    document.getElementById('camera-placeholder').style.display = 'block';
    document.getElementById('btn-start-camera').disabled = false;
    document.getElementById('btn-capture').disabled = true;
    document.getElementById('btn-stop-camera').disabled = true;
}

// ========== Capture & Recognize ==========
async function captureAndRecognize() {
    const video = document.getElementById('camera-preview');
    if (!mediaStream || !video.videoWidth) {
        showError('result-alert', '摄像头未就绪');
        return;
    }

    // Capture frame to canvas
    const canvas = document.createElement('canvas');
    canvas.width = video.videoWidth;
    canvas.height = video.videoHeight;
    const ctx = canvas.getContext('2d');
    ctx.drawImage(video, 0, 0);

    // Convert to base64 JPEG
    const imageData = canvas.toDataURL('image/jpeg', 0.9);

    // Show loading state
    const resultContent = document.getElementById('result-content');
    const resultPlaceholder = document.getElementById('result-placeholder');
    resultPlaceholder.style.display = 'none';
    resultContent.style.display = 'block';
    document.getElementById('result-plate').textContent = '识别中...';
    document.getElementById('result-actions').style.display = 'none';
    document.getElementById('result-alert').innerHTML = '';

    // Hide previous capture preview
    const oldPreview = document.getElementById('capture-preview');
    if (oldPreview) oldPreview.remove();

    // Send to server for recognition
    const res = await post('/api/plate/recognize-image', { image: imageData });

    if (res && res.ok) {
        displayResult(res.data);
        addToHistory(res.data);
    } else {
        document.getElementById('result-plate').textContent = '识别失败';
        showError('result-alert', res?.data?.error || '识别请求失败');
    }
}

function displayResult(data) {
    const plate = data.plate_number || '未识别';
    const confidence = data.confidence || 0;
    const color = data.color || 'unknown';
    const msg = data.recognize_message || '';

    document.getElementById('result-plate').textContent = plate;
    document.getElementById('result-confidence').textContent =
        (confidence * 100).toFixed(1) + '%' +
        (confidence < 0.5 ? ' (较低)' : confidence < 0.7 ? ' (中等)' : ' (较高)');
    document.getElementById('result-color').textContent = getColorLabel(color);

    lastRecognizedPlate = plate;

    // Display registration info
    const regContainer = document.getElementById('registration-info');
    if (data.registration) {
        const reg = data.registration;
        let html = '';
        if (reg.is_blacklisted) {
            html += `<div class="alert alert-error" style="margin-bottom:10px">
                <strong>黑名单车辆</strong><br>${escapeHtml(reg.blacklist_reason || '已被列入黑名单')}</div>`;
        } else if (reg.is_registered) {
            html += `<div class="alert alert-success" style="margin-bottom:10px">✅ 该车辆已登记</div>`;
            if (reg.in_parking) {
                html += `<p style="font-size:13px;color:#ff4d4f;margin-bottom:4px">
                    <strong>当前状态:</strong> 在场内（入库: ${formatDateTime(reg.last_check_in)}）</p>`;
            } else {
                html += `<p style="font-size:13px;color:#52c41a;margin-bottom:4px">
                    <strong>当前状态:</strong> 不在场内</p>`;
            }
            if (reg.has_monthly_pass) {
                html += `<p style="font-size:13px;color:#1890ff;margin-bottom:4px">
                    <strong>月卡:</strong> 有效至 ${reg.monthly_pass_end}</p>`;
            }
        } else {
            html += `<div class="alert alert-warning" style="margin-bottom:10px">⚠️ 该车辆未登记</div>`;
            html += `<p style="font-size:13px;color:#666">系统中未找到该车牌号的登记信息，请先办理登记手续。</p>`;
        }
        html += `<p style="font-size:12px;color:#999;margin-top:8px">${escapeHtml(reg.message || '')}</p>`;
        regContainer.innerHTML = html;

        // Show quick actions 
        const actions = document.getElementById('result-actions');
        if (hasPerm('vehicle.checkin') || hasPerm('vehicle.checkout')) {
            actions.style.display = 'flex';
            const btnIn = document.getElementById('btn-quick-checkin');
            const btnOut = document.getElementById('btn-quick-checkout');
            if (btnIn) btnIn.style.display = hasPerm('vehicle.checkin') && !reg.in_parking ? '' : 'none';
            if (btnOut) btnOut.style.display = hasPerm('vehicle.checkout') && reg.in_parking ? '' : 'none';
        }
    } else {
        regContainer.innerHTML = '<p style="color:#999;font-size:13px">登记信息查询失败</p>';
    }
}

function getColorLabel(color) {
    const map = {
        '蓝牌': '蓝牌',
        '绿牌（新能源）': '绿牌（新能源）',
        '黄牌': '黄牌',
        'blue': '蓝牌',
        'green': '绿牌（新能源）',
        'yellow': '黄牌',
        'unknown': '未识别',
        '未知': '未识别'
    };
    return map[color] || color || '未识别';
}

// ========== Quick Check-in/out ==========
async function quickCheckin() {
    if (!lastRecognizedPlate) return;
    const res = await post('/api/vehicle/checkin', {
        license_plate: lastRecognizedPlate,
        billing_type: 'standard'
    });
    if (res && res.ok) {
        showSuccess('result-alert', `车辆 ${lastRecognizedPlate} 入库成功！`);
        document.getElementById('btn-quick-checkin').style.display = 'none';
        document.getElementById('btn-quick-checkout').style.display = '';
    } else {
        showError('result-alert', res?.data?.error || '入库失败');
    }
}

async function quickCheckout() {
    if (!lastRecognizedPlate) return;
    const res = await post('/api/vehicle/checkout', {
        license_plate: lastRecognizedPlate
    });
    if (res && res.ok) {
        showSuccess('result-alert', `车辆 ${lastRecognizedPlate} 出库成功！费用: ${formatFee(res.data.fee)}`);
        document.getElementById('btn-quick-checkout').style.display = 'none';
        document.getElementById('btn-quick-checkin').style.display = '';
    } else {
        showError('result-alert', res?.data?.error || '出库失败');
    }
}

// ========== Manual Check ==========
async function manualCheck() {
    const input = document.getElementById('manual-plate-input');
    const plate = input.value.trim().toUpperCase();
    if (!plate) { showError('manual-result', '请输入车牌号'); return; }

    const container = document.getElementById('manual-result');
    container.innerHTML = '<p style="color:#999">查询中...</p>';

    const res = await post('/api/plate/check-registered', { license_plate: plate });
    if (res && res.ok) {
        const reg = res.data;
        let html = `<div style="padding:10px;background:#f5f5f5;border-radius:6px;font-size:13px">
            <p><strong>${escapeHtml(reg.plate_number)}</strong></p>`;
        if (reg.is_blacklisted) {
            html += `<p style="color:#ff4d4f">黑名单: ${escapeHtml(reg.blacklist_reason)}</p>`;
        } else if (reg.is_registered) {
            html += `<p style="color:#52c41a">✅ 已登记</p>`;
            if (reg.in_parking) html += `<p>状态: 在场内 (${formatDateTime(reg.last_check_in)})</p>`;
            if (reg.has_monthly_pass) html += `<p>月卡有效至: ${reg.monthly_pass_end}</p>`;
        } else {
            html += `<p style="color:#faad14">⚠️ 未登记</p>`;
        }
        html += `<p style="color:#999;font-size:12px;margin-top:4px">${escapeHtml(reg.message)}</p>`;
        html += '</div>';
        container.innerHTML = html;
    } else {
        container.innerHTML = `<div class="alert alert-error">查询失败: ${res?.data?.error || '请求错误'}</div>`;
    }
}

// ========== History ==========
function addToHistory(data) {
    const container = document.getElementById('history-container');
    const entries = JSON.parse(localStorage.getItem('recognize_history') || '[]');

    entries.unshift({
        plate: data.plate_number || '未识别',
        time: new Date().toISOString(),
        confidence: data.confidence || 0,
        registered: data.registration ? data.registration.is_registered : false
    });

    // Keep last 20 entries
    if (entries.length > 20) entries.length = 20;
    localStorage.setItem('recognize_history', JSON.stringify(entries));

    renderHistory();
}

function renderHistory() {
    const container = document.getElementById('history-container');
    const entries = JSON.parse(localStorage.getItem('recognize_history') || '[]');

    if (entries.length === 0) {
        container.innerHTML = '<p style="color:#999;font-size:13px">暂无识别记录</p>';
        return;
    }

    container.innerHTML = entries.map(e => `
        <div style="display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid #f0f0f0;font-size:13px">
            <div>
                <strong>${escapeHtml(e.plate)}</strong>
                ${e.registered
                    ? '<span class="badge badge-success" style="margin-left:6px">已登记</span>'
                    : '<span class="badge badge-warning" style="margin-left:6px">未登记</span>'}
                <span style="color:#bbb;font-size:11px;margin-left:6px">(${(e.confidence*100).toFixed(0)}%)</span>
            </div>
            <span style="color:#bbb;font-size:11px">${formatDateTime(e.time)}</span>
        </div>
    `).join('');
}

// ========== Keyboard shortcut ==========
document.getElementById('manual-plate-input')?.addEventListener('keydown', e => {
    if (e.key === 'Enter') manualCheck();
});

// Init
renderHistory();
setActiveNav('nav-recognize');

// Check permission
if (!hasPerm('plate.recognize')) {
    document.querySelector('.main-content').innerHTML =
        '<div style="text-align:center;padding:80px 20px"><h2>权限不足</h2><p style="color:#999">您没有车牌识别的权限</p></div>';
}
