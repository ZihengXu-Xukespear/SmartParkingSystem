const user = checkAuth();
if (user) initSidebar();

function loadProfile() {
    document.getElementById('prof-username').value = user.username || '';
    document.getElementById('prof-phone').value = user.telephone || '';
    document.getElementById('prof-truename').value = user.truename || '';
    document.getElementById('prof-role').value = (user.role === 'admin' || user.role === 'root' ? '管理员' : '普通用户');
}

async function updateProfile() {
    const phone = document.getElementById('prof-phone').value.trim();
    const truename = document.getElementById('prof-truename').value.trim();
    const password = document.getElementById('prof-new-pass').value;
    const body = { id: user.id, username: user.username, telephone: phone, truename, role: user.role };
    if (password) body.password = password;

    const res = await put('/api/user/update', body);
    if (res && res.ok) {
        // Update stored user
        user.telephone = phone;
        user.truename = truename;
        sessionStorage.setItem('user', JSON.stringify(user));
        initSidebar();
        showSuccess('alert-box', '个人信息已更新');
    } else {
        showError('alert-box', res?.data?.error || '更新失败');
    }
}

async function loadBalance() {
    if (!hasPerm('balance.view')) return;
    const res = await get('/api/balance');
    const balEl = document.getElementById('prof-balance');
    const txEl = document.getElementById('prof-transactions');
    if (res && res.ok) {
        if (balEl) balEl.textContent = '¥' + parseFloat(res.data.balance).toFixed(2);
        if (txEl && res.data.transactions) {
            txEl.innerHTML = res.data.transactions.slice(0, 20).map(t =>
                `<div style="padding:4px 0;border-bottom:1px solid #f0f0f0;font-size:12px">
                    <span style="color:${t.amount>0?'#52c41a':'#ff4d4f'}">${t.amount>0?'+':''}${parseFloat(t.amount).toFixed(2)}</span>
                    <span style="color:#999;margin-left:8px">${escapeHtml(t.description)}</span>
                    <span style="float:right;color:#bbb;font-size:11px">${formatDateTime(t.created_at)}</span>
                </div>`
            ).join('') || '<p style="color:#999;font-size:12px">暂无交易记录</p>';
        }
    }
}

async function loadPasses() {
    const container = document.getElementById('prof-passes');
    if (!container) return;
    const res = await get('/api/parking/monthly-passes');
    if (!res || !res.ok || !res.data.passes) { container.innerHTML = '<p style="color:#999">暂无月卡</p>'; return; }
    const myPasses = res.data.passes.filter(p => p.user_id === user.id);
    if (myPasses.length === 0) { container.innerHTML = '<p style="color:#999">暂无月卡</p>'; return; }
    container.innerHTML = myPasses.map(p =>
        `<div style="padding:8px;border-bottom:1px solid #f0f0f0">
            <strong>${escapeHtml(p.license_plate)}</strong>
            <span style="color:#666"> ${escapeHtml(p.pass_type)}</span>
            <div style="font-size:11px;color:#999">${formatDate(p.start_date)} ~ ${formatDate(p.end_date)} | ${formatFee(p.fee)}</div>
            <span class="badge ${p.is_active?'badge-success':'badge-danger'}" style="font-size:10px">${p.is_active?'有效':'过期'}</span>
        </div>`
    ).join('');
}

loadProfile();
loadBalance();
loadPasses();
