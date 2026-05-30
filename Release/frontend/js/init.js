// Check system status on page load
(async function() {
    const res = await get('/api/init/status');
    if (res && res.ok && res.data.initialized) {
        document.getElementById('section-new').style.display = 'none';
        document.getElementById('section-ready').style.display = '';
        document.getElementById('ready-parking-name').textContent =
            '停车场: ' + res.data.parking_name + ' | 端口: ' + res.data.server_port;
    }
})();

// ========== Fresh init ==========
async function handleInit() {
    const config = {
        host: document.getElementById('host').value.trim(),
        port: parseInt(document.getElementById('port').value),
        database: document.getElementById('database').value.trim(),
        user: document.getElementById('user').value.trim(),
        password: document.getElementById('password').value,
        parking_name: document.getElementById('parking_name').value.trim(),
        fee: parseFloat(document.getElementById('fee').value),
        capacity: parseInt(document.getElementById('capacity').value),
        server_port: parseInt(document.getElementById('server_port').value)
    };

    if (!config.password) { showError('alert-box', '请输入数据库密码'); return; }
    if (!config.parking_name) { showError('alert-box', '请输入停车场名称'); return; }

    if (!confirm(
        '确认初始化系统？\n\n' +
        '数据库: ' + config.host + ':' + config.port + '/' + config.database + '\n' +
        '停车场: ' + config.parking_name + ' (' + config.capacity + ' 车位)\n' +
        '费率: ' + config.fee + ' 元/小时\n\n' +
        '默认管理员: admin / admin123'
    )) return;

    const btn = document.getElementById('init-btn');
    btn.disabled = true; btn.textContent = '正在初始化...';

    const res = await post('/api/init/database', config);

    if (res && res.ok) {
        showSuccess('alert-box', '初始化完成！正在跳转...');
        setTimeout(() => window.location.href = '/index.html', 1500);
    } else {
        btn.disabled = false; btn.textContent = '开始初始化';
        showError('alert-box', res?.data?.error || '初始化失败');
    }
}

// ========== Re-init ==========
function toggleReinit() {
    const section = document.getElementById('reinit-section');
    section.classList.toggle('show');
}

async function handleReinit() {
    const username = document.getElementById('reinit-user').value.trim();
    const password = document.getElementById('reinit-pass').value;

    if (!username || !password) { showError('reinit-alert-box', '请输入管理员凭据'); return; }

    // Step 1: login
    let loginRes = await post('/api/auth/login', { username, password });
    if (!loginRes || !loginRes.ok) {
        showError('reinit-alert-box', loginRes?.data?.error || '验证失败');
        return;
    }
    if (loginRes.data.user.role !== 'admin') {
        showError('reinit-alert-box', '需要管理员权限');
        return;
    }

    // Step 2: confirm
    if (!confirm('将清空所有数据并重新初始化，不可撤销。确定？')) return;
    if (!confirm('再次确认：所有用户、记录、月卡数据将永久丢失。')) return;

    const token = loginRes.data.token;
    const btn = document.getElementById('reinit-btn');
    btn.disabled = true; btn.textContent = '重置中...';

    const res = await request('/api/init/database', {
        method: 'POST',
        headers: { 'Authorization': 'Bearer ' + token }
    });

    if (res && res.ok) {
        sessionStorage.clear();
        showSuccess('reinit-alert-box', '已重置！即将刷新...');
        setTimeout(() => location.reload(), 1500);
    } else {
        btn.disabled = false; btn.textContent = '确认重置';
        showError('reinit-alert-box', res?.data?.error || '重置失败');
    }
}
