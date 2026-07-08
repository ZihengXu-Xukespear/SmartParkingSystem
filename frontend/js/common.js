
const API_BASE = '';
async function request(url, options = {}) {
    const token = sessionStorage.getItem('token');
    const headers = {
        'Content-Type': 'application/json',
        ...options.headers
    };
    if (token) {
        headers['Authorization'] = 'Bearer ' + token;
    }
    try {
        const resp = await fetch(API_BASE + url, { ...options, headers });
        const data = await resp.json();
                if (resp.status === 401 && token && !url.includes('/api/auth/login')) {
            sessionStorage.removeItem('token');
            sessionStorage.removeItem('user');
            sessionStorage.removeItem('permissions');
            window.location.href = '/index.html';
            return null;
        }
        return { ok: resp.ok, status: resp.status, data };
    } catch (e) {
        console.error('Request failed:', e);
        return { ok: false, status: 0, data: { error: '网络请求失败' } };
    }
}
async function get(url) {
    return request(url, { method: 'GET' });
}
async function post(url, body) {
    return request(url, { method: 'POST', body: JSON.stringify(body) });
}
async function put(url, body) {
    return request(url, { method: 'PUT', body: JSON.stringify(body) });
}
async function del(url) {
    return request(url, { method: 'DELETE' });
}
function checkAuth() {
    const token = sessionStorage.getItem('token');
    const user = sessionStorage.getItem('user');
    if (!token || !user) {
        window.location.href = '/index.html';
        return null;
    }
    return JSON.parse(user);
}
function getUser() {
    const user = sessionStorage.getItem('user');
    return user ? JSON.parse(user) : null;
}
function showAlert(containerId, message, type = 'info') {
    const container = document.getElementById(containerId);
    if (!container) return;
    container.innerHTML = `<div class="alert alert-${type}">${message}</div>`;
    setTimeout(() => { container.innerHTML = ''; }, 5000);
}
function showSuccess(containerId, message) { showAlert(containerId, message, 'success'); }
function showError(containerId, message) { showAlert(containerId, message, 'error'); }
function showWarning(containerId, message) { showAlert(containerId, message, 'warning'); }
function showModal(id) {
    document.getElementById(id).classList.add('show');
}
function hideModal(id) {
    document.getElementById(id).classList.remove('show');
}
function formatDateTime(dt) {
    if (!dt) return '-';
    return dt.replace('T', ' ').substring(0, 19);
}
function formatDate(d) {
    return d ? d.substring(0, 10) : '-';
}
function computeDurationText(startStr, endStr) {
    if (!startStr || !endStr) return '';
    const s = new Date(String(startStr).replace(' ', 'T'));
    const e = new Date(String(endStr).replace(' ', 'T'));
    if (isNaN(s) || isNaN(e) || e <= s) return '';
    const mins = Math.floor((e - s) / 60000);
    return Math.floor(mins / 60) + '小时' + (mins % 60) + '分';
}
function escapeHtml(str) {
    if (!str) return '';
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

// 格式化金额
function formatFee(fee) {
    if (fee === null || fee === undefined) return '-';
    return '¥' + parseFloat(fee).toFixed(2);
}

// 侧边栏高亮
function setActiveNav(id) {
    document.querySelectorAll('.nav-item').forEach(el => el.classList.remove('active'));
    const target = document.getElementById(id);
    if (target) target.classList.add('active');
}

// Permission checking
function getUserPermissions() {
    const perms = sessionStorage.getItem('permissions');
    return perms ? JSON.parse(perms) : [];
}

function hasPerm(permission) {
    return getUserPermissions().includes(permission);
}

// 退出登录
async function logout() {
    await post('/api/auth/logout', {});
    sessionStorage.removeItem('token');
    sessionStorage.removeItem('user');
    sessionStorage.removeItem('permissions');
    window.location.href = '/index.html';
}

// 初始化侧边栏用户信息
function initSidebar() {
    const user = getUser();
    if (!user) return;

    const nameEl = document.querySelector('.user-name');
    const roleEl = document.querySelector('.user-role');
    const avatarEl = document.querySelector('.user-avatar');

    if (nameEl) nameEl.textContent = user.truename || user.username;
    if (roleEl) {
        const roleLabels = { admin: '管理员', root: '管理员', user: '普通用户' };
        roleEl.textContent = roleLabels[user.role] || '普通用户';
    }
    if (avatarEl) avatarEl.textContent = (user.truename || user.username).charAt(0).toUpperCase();

    // Hide admin nav if user doesn't have user management permission
    if (!hasPerm('user.view')) {
        const adminNav = document.getElementById('nav-admin');
        if (adminNav) adminNav.style.display = 'none';
    }
    // Hide vehicle nav if user doesn't have vehicle query permission
    if (!hasPerm('vehicle.query')) {
        const vehicleNav = document.getElementById('nav-vehicles');
        if (vehicleNav) vehicleNav.style.display = 'none';
    }
    if (!hasPerm('parking.settings')) {
        const parkingNav = document.getElementById('nav-parking');
        if (parkingNav) parkingNav.style.display = 'none';
    }
    if (!hasPerm('vehicle.checkin')) {
        const checkinNav = document.getElementById('nav-checkin');
        if (checkinNav) checkinNav.style.display = 'none';
    }
    if (!hasPerm('plate.recognize')) {
        const recognizeNav = document.getElementById('nav-recognize');
        if (recognizeNav) recognizeNav.style.display = 'none';
    }

    // Role-based nav visibility:
    // Regular users: hide admin-only items (车辆入库, 车牌识别)
    // Admin: hide user-only items (智能入库, 计费规则)
    if (user.role === 'user') {
        // Hide admin-only pages for regular users
        const checkinNav = document.getElementById('nav-checkin');
        if (checkinNav) checkinNav.style.display = 'none';
        const recognizeNav = document.getElementById('nav-recognize');
        if (recognizeNav) recognizeNav.style.display = 'none';
    } else {
        // Hide user-only pages for admin
        const userCheckinNav = document.getElementById('nav-user-checkin');
        if (userCheckinNav) userCheckinNav.style.display = 'none';
        const billingNav = document.getElementById('nav-billing');
        if (billingNav) billingNav.style.display = 'none';
    }

    // Adjust chat nav based on role: users -> AI chat, admins -> conversations overview
    const chatNav = document.getElementById('nav-chat');
    if (chatNav) {
        if (!hasPerm('message.send')) {
            chatNav.style.display = 'none';
        } else if (user.role === 'admin' || user.role === 'root') {
            const iconSpan = chatNav.querySelector('.icon');
            if (iconSpan) iconSpan.textContent = '💬';
            chatNav.lastChild.textContent = ' 客服会话';
            chatNav.href = '/conversations.html';
        }
    }
}
