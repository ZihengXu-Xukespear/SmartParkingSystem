// ======================= 个人中心逻辑 =======================

// 检查登录，如果未登录会跳转；若已登录会返回用户对象
const user = checkAuth();
if (user) initSidebar();

// ========== 个人信息 ==========
function loadProfile() {
    document.getElementById('prof-username').value = user.username || '';
    document.getElementById('prof-phone').value = user.telephone || '';
    document.getElementById('prof-truename').value = user.truename || '';
    // 角色显示中文
    const roleMap = {root:'超级管理员', admin:'管理员', operator:'操作员', user:'普通用户'};
    document.getElementById('prof-role').value = roleMap[user.role] || '普通用户';
}

// ========== 更新个人信息（含密码） ==========
async function updateProfile() {
    const phone = document.getElementById('prof-phone').value.trim();
    const truename = document.getElementById('prof-truename').value.trim();
    const password = document.getElementById('prof-new-pass').value;

    const body = {
        id: user.id,
        username: user.username,
        telephone: phone,
        truename: truename,
        role: user.role
    };
    if (password) body.password = password;

    const res = await put('/api/user/update', body);
    if (res && res.ok) {
        // 更新本地存储的用户信息
        user.telephone = phone;
        user.truename = truename;
        localStorage.setItem('user', JSON.stringify(user));
        initSidebar(); // 刷新侧边栏用户展示
        showSuccess('alert-box', '个人信息已更新');
    } else {
        showError('alert-box', res?.data?.error || '更新失败');
    }
}

// ========== 我的余额 ==========
async function loadBalance() {
    if (!hasPerm('balance.view')) return;
    const res = await get('/api/balance');
    const balEl = document.getElementById('prof-balance');
    const txEl = document.getElementById('prof-transactions');
    if (res && res.ok) {
        if (balEl) balEl.textContent = '¥' + parseFloat(res.data.balance).toFixed(2);
        if (txEl && res.data.transactions) {
            txEl.innerHTML = res.data.transactions.slice(0, 20).map(t => `
                <div style="padding:4px 0; border-bottom:1px solid #f0f0f0; font-size:12px">
                    <span style="color:${t.amount>0?'#52c41a':'#ff4d4f'}">
                        ${t.amount>0?'+':''}${parseFloat(t.amount).toFixed(2)}
                    </span>
                    <span style="color:#999; margin-left:8px">${escapeHtml(t.description)}</span>
                    <span style="float:right; color:#bbb; font-size:11px">${formatDateTime(t.created_at)}</span>
                </div>
            `).join('') || '<p style="color:#999; font-size:12px">暂无交易记录</p>';
        }
    }
}

// ========== 我的套餐（已购买的月卡） ==========
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
            <span class="badge ${p.is_active?'badge-success':'badge-danger'}" style="font-size:10px">
                ${p.is_active?'有效':'过期'}
            </span>
        </div>
    `).join('');
}

// ========== 套餐购买（新增功能） ==========
async function loadProfilePlans() {
    const container = document.getElementById('profile-plans-container');
    if (!container || !hasPerm('balance.view')) return;
    const res = await get('/api/pass-plans');
    if (!res || !res.ok || !res.data.plans || res.data.plans.length === 0) {
        container.innerHTML = '<p style="color:#999">暂无可用套餐</p>';
        return;
    }
    container.innerHTML = res.data.plans.map(p => `
        <div style="border:1px solid #e8e8e8; border-radius:6px; padding:10px; margin-bottom:8px;
                    display:flex; justify-content:space-between; align-items:center;
                    transition:all 0.2s"
             onmouseover="this.style.borderColor='#1890ff';this.style.boxShadow='0 2px 8px rgba(24,144,255,0.15)'"
             onmouseout="this.style.borderColor='#e8e8e8';this.style.boxShadow='none'">
            <div>
                <strong>${escapeHtml(p.plan_name)}</strong>
                <span style="color:#999; font-size:12px; margin-left:6px">${p.duration_days}天</span>
                ${p.description ? '<div style="color:#666; font-size:12px">'+escapeHtml(p.description)+'</div>' : ''}
            </div>
            <div style="text-align:right">
                <span style="color:#ff4d4f; font-weight:bold; font-size:16px">¥${p.price.toFixed(2)}</span>
                <br>
                <button class="btn btn-primary btn-xs" style="margin-top:4px"
                        onclick="profilePurchasePlan(${p.id}, '${escapeHtml(p.plan_name)}', ${p.price})">
                    购买
                </button>
            </div>
        </div>
    `).join('');
}

async function profilePurchasePlan(planId, planName, price) {
    const plate = prompt(`购买【${planName}】（¥${price.toFixed(2)}），请输入车牌号：`);
    if (!plate) return;
    
    // 简单校验车牌格式（可以调用后端验证接口）
    const valRes = await post('/api/plate/validate', { license_plate: plate });
    if (valRes && valRes.ok && !valRes.data.valid) {
        alert('车牌号格式不正确：' + (valRes.data.message || ''));
        return;
    }

    const res = await post('/api/pass-plans/' + planId + '/purchase', { license_plate: plate });
    if (res && res.ok) {
        alert('购买成功！');
        loadBalance();      // 刷新余额
        loadPasses();       // 刷新已购套餐
        loadProfilePlans(); // 重新渲染购买列表（可选）
    } else {
        alert(res?.data?.error || '购买失败，请确认余额是否充足');
    }
}

// ========== 页面初始化 ==========
loadProfile();
loadBalance();
loadPasses();
loadProfilePlans();
