// 登录页面逻辑

let loginMode = 'user'; // 'user' or 'admin'

function switchLoginTab(mode) {
    loginMode = mode;
    document.getElementById('tab-user').classList.toggle('active', mode === 'user');
    document.getElementById('tab-admin').classList.toggle('active', mode === 'admin');

    const subtitle = document.getElementById('login-subtitle');
    const footer = document.getElementById('auth-footer');
    const usernameLabel = document.getElementById('label-username');

    if (mode === 'admin') {
        subtitle.textContent = '请输入管理员账户信息';
        usernameLabel.textContent = '管理员账号';
        footer.innerHTML = '<a href="/init.html">系统初始化</a>';
    } else {
        subtitle.textContent = '请输入您的账户信息';
        usernameLabel.textContent = '用户名';
        footer.innerHTML = '还没有账号？<a href="/register.html">立即注册</a>&nbsp;|&nbsp;<a href="/init.html">系统初始化</a>';
    }

    document.getElementById('username').value = '';
    document.getElementById('password').value = '';
    document.getElementById('alert-box').innerHTML = '';
}

async function handleLogin() {
    const btn = document.getElementById('login-btn');
    const username = document.getElementById('username').value.trim();
    const password = document.getElementById('password').value;

    if (!username || !password) {
        showError('alert-box', '用户名和密码不能为空');
        return;
    }

    btn.disabled = true;
    btn.textContent = '登录中...';

    const res = await post('/api/auth/login', { username, password });

    btn.disabled = false;
    btn.textContent = '登 录';

    if (res && res.ok) {
        const userRole = res.data.user.role;

        if (loginMode === 'admin' && userRole !== 'admin' && userRole !== 'root') {
            showError('alert-box', '该账户不是管理员，请使用用户登录');
            await post('/api/auth/logout', {});
            return;
        }

        sessionStorage.setItem('token', res.data.token);
        sessionStorage.setItem('user', JSON.stringify(res.data.user));
        if (res.data.permissions) {
            sessionStorage.setItem('permissions', JSON.stringify(res.data.permissions));
        }
        showSuccess('alert-box', '登录成功，正在跳转...');
        setTimeout(() => window.location.href = '/dashboard.html', 800);
    } else {
        const modeLabel = loginMode === 'admin' ? '管理员' : '';
        showError('alert-box', res?.data?.error || (modeLabel + '登录失败'));
    }
}

// Enter key support
document.addEventListener('keydown', e => {
    if (e.key === 'Enter') handleLogin();
});
