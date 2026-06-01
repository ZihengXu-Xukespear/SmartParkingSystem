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

// 加载用户套餐（月卡/季卡/年卡 统一显示）
async function loadPasses() {
    const container = document.getElementById('prof-passes');
    if (!container) return;

    try {
        // 请求接口（你原有接口不变）
        const res = await get(`/api/parking/monthly-passes?user_id=${user.id}`);

        // 请求失败 / 无数据
        if (!res || !res.ok || !res.data.passes) {
            container.innerHTML = '<div class="package-empty">暂无套餐</div>';
            return;
        }

        // 筛选当前用户的套餐
        const myPackages = res.data.passes.filter(p => p.user_id === user.id);

        // 无套餐
        if (myPackages.length === 0) {
            container.innerHTML = '<div class="package-empty">暂无套餐</div>';
            return;
        }

        // 渲染套餐列表
        container.innerHTML = myPackages.map(item => `
            <div class="package-item">
                <div class="package-type">
                    ${item.license_plate} • ${escapeHtml(item.pass_type)}
                </div>
                ${item.P_name ? `<div style="font-size:12px;color:#1890ff;margin-bottom:2px">停车场：${escapeHtml(item.P_name)}</div>` : ''}
                <div class="package-time">
                    生效时间：${formatDate(item.start_date)}<br>
                    到期时间：${formatDate(item.end_date)}
                </div>
                <div style="margin-top:6px;display:flex;justify-content:space-between;align-items:center">
                    <span style="font-size:12px;color:#666">${formatFee(item.fee)}</span>
                    <span class="badge ${item.is_active ? 'badge-success' : 'badge-danger'}" style="font-size:10px">
                        ${item.is_active ? '有效' : '已过期'}
                    </span>
                </div>
            </div>
        `).join('');

    } catch (err) {
        console.error('加载套餐失败：', err);
        container.innerHTML = '<div class="package-empty">加载失败</div>';
    }
}

loadProfile();
loadBalance();
loadPasses();
