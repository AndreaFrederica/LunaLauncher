/* Local interaction model only. No OCS requests, filesystem writes or launcher APIs. */
'use strict';
const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => [...document.querySelectorAll(selector)];
const escapeHtml = (value) => String(value).replace(/[&<>"']/g, char => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[char]));
const icon = name => `<i data-icon="${name}" aria-hidden="true"></i>`;
function hydrateIcons(root = document) {
  root.querySelectorAll('[data-icon]').forEach(node => {
    node.style.setProperty('--icon', `url("../../neo-ui/src/pcl/icons/${node.dataset.icon}.svg")`);
    node.setAttribute('aria-hidden', 'true');
  });
}
const labels = {palette:'配色',icons:'图标',skin:'控件',decoration:'装饰',bundle:'成套'};
const compatibility = {ready:'格式可识别',partial:'部分可用',unsupported:'当前平台不支持'};
const colors = {
  light: {canvas:'#e0eaf5',paper:'#ffffff',bar:'#f0f4f9',fg:'#33475d',color:'#4f8dca'},
  night: {canvas:'#28364b',paper:'#293443',bar:'#212b38',fg:'#e2e8f1',color:'#78acd6',buttonfg:'#152238'},
  rose: {canvas:'#f0e5e8',paper:'#fffafa',bar:'#f7eff1',fg:'#675461',color:'#b47893'},
  sand: {canvas:'#eee8dc',paper:'#fffdf8',bar:'#f5f1e6',fg:'#665d4b',color:'#a68b55'},
  mint: {canvas:'#e0eae5',paper:'#fbfdfb',bar:'#edf4ef',fg:'#425d51',color:'#619a7d'},
  ocean: {canvas:'#263f53',paper:'#203144',bar:'#192838',fg:'#d5e4ef',color:'#4b9fc1'},
  violet: {canvas:'#e8e4f3',paper:'#fcfbff',bar:'#f1eef8',fg:'#554d6e',color:'#8c79bf'},
};
const file = (id,name,components,size='5.2 KiB',palette='light') => ({id,name,components,size,palette,version:'1.2',date:'2026-10-20'});
// All names, versions, dates and file entries below are fixtures, not store metadata.
const themes = [
  {id:'glacier',name:'Glacier Blue',kind:'palette',palette:'light',compat:'ready',description:'干净的冷色界面，柔和的蓝色用于突出当前选择。提供浅色与深色两个独立文件。',files:[file('light','Glacier-Light.colors',['palette']),file('dark','Glacier-Dark.colors',['palette'],'5.6 KiB','night')]},
  {id:'nordic',name:'Nordic Mist',kind:'palette',palette:'night',compat:'ready',description:'低饱和的深色配色，适合长时间查看实例和日志。',files:[file('dark','Nordic-Mist.colors',['palette'],'6.1 KiB','night')]},
  {id:'rose',name:'Rose Dusk',kind:'palette',palette:'rose',compat:'ready',description:'温暖的浅色底与玫瑰色强调色。只改变配色，保留你选择的控件和窗口装饰。',files:[file('light','Rose-Dusk.colors',['palette'],'4.8 KiB','rose')]},
  {id:'aurora',name:'Aurora Icons',kind:'icons',palette:'light',compat:'ready',description:'清晰的双色图标示例。缺少的启动器专属图标将回退到内置资源。',files:[file('icons','Aurora-Icons.tar.xz',['icons'],'8.4 MiB')]},
  {id:'sand',name:'Sandstone',kind:'palette',palette:'sand',compat:'ready',description:'温和的米色与沙金色强调色。',files:[file('light','Sandstone.colors',['palette'],'4.9 KiB','sand')]},
  {id:'mint',name:'Mint Leaf',kind:'palette',palette:'mint',compat:'ready',description:'明亮而清爽的绿色配色示例。',files:[file('light','Mint-Leaf.colors',['palette'],'5.1 KiB','mint')]},
  {id:'ocean',name:'Ocean Night',kind:'palette',palette:'ocean',compat:'ready',description:'深海蓝背景与青色强调色。',files:[file('dark','Ocean-Night.colors',['palette'],'5.8 KiB','ocean')]},
  {id:'nebula',name:'Nebula Desktop',kind:'bundle',palette:'violet',compat:'partial',description:'Plasma 成套主题示例。当前启动器能提取其中的配色，其他桌面组件保持不可选。',files:[file('bundle','Nebula-Plasma6.tar.gz',['palette'],'12.8 MiB','violet')]},
  {id:'glass',name:'Glass Aurorae',kind:'decoration',palette:'ocean',compat:'unsupported',description:'依赖 KWin / Aurorae 的窗口装饰，当前 Windows 能力不支持直接应用。',files:[file('aurorae','Glass-Aurorae.tar.gz',[],'1.5 MiB','ocean')]},
  {id:'kvantum',name:'Velvet Kvantum',kind:'skin',palette:'rose',compat:'unsupported',description:'需要 Kvantum 渲染引擎。当前构建未声明此引擎，无法安装使用。',files:[file('kv','Velvet-Kvantum.tar.gz',[],'580 KiB','rose')]},
  {id:'luna-arc',provider:'github',name:'Luna Arc',kind:'bundle',palette:'light',compat:'ready',description:'Luna 主题格式示例：控件皮肤与浅色配色可以分别选用。更换控件皮肤需要重新启动。',files:[file('arc','Luna-Arc-theme.zip',['palette','skin'],'720 KiB')]},
].map((theme,index) => ({provider:'kde',author:'示例作者',license:'示例数据，许可待真实来源提供',order:index,...theme}));
const baseline = {mode:'independent',style:'Fusion',skin:'none',palette:'builtin',icons:'builtin',decoration:'system'};
const state = {
  page:'discover',provider:'kde',category:'all',query:'',sort:'featured',compatible:true,pageIndex:0,pageSize:4,
  selected:'glacier',selectedFiles:new Map(),scenario:'normal',tasks:[],sequence:0,queryGeneration:0,
  installed:new Map([['builtin',{key:'builtin',name:'Luna Light',provider:'内置',palette:'light',components:['palette','icons'],version:'内置'}]]),
  effective:{...baseline},pending:null,draft:{...baseline},revision:1,
};
function themeById(id){return themes.find(theme => theme.id === id);}
function selectedFile(theme){return theme.files.find(item => item.id === state.selectedFiles.get(theme.id)) || theme.files[0];}
function packageKey(theme,selected){return `${theme.id}:${selected.id}`;}
function activeTask(key){return state.tasks.find(task => task.key === key && task.status === 'running');}
function badge(theme){return `<span class="badge ${theme.compat === 'ready' ? '' : theme.compat}">${compatibility[theme.compat]}</span>`;}
function preview(theme,fileInfo = null,chrome = 'system',skin = 'none') {
  const palette = colors[fileInfo?.palette || theme.palette] || colors.light;
  const css = Object.entries(palette).map(([key,value]) => `--${key}:${value}`).join(';');
  if(theme.kind === 'icons') return `<div class="icon-preview" style="${css}">${['folder-open','box','palette','settings','globe','download','layers','archive'].map(icon).join('')}</div>`;
  return `<div class="preview-surface" style="${css}" aria-hidden="true"><div class="mini-window" style="${skin !== 'none' ? 'border-radius:10px 10px 0 0' : ''}"><div class="mini-titlebar"><span class="mini-app-mark"></span>Luna Launcher<span class="window-controls">${chrome === 'luna' ? '○ ○ ×' : '− □ ×'}</span></div><div class="mini-body"><div class="mini-nav"><div class="mini-line active"></div><div class="mini-line"></div><div class="mini-line"></div><div class="mini-line"></div></div><div class="mini-main"><strong>我的实例</strong><div class="mini-grid"><span class="mini-tile"></span><span class="mini-tile"></span><span class="mini-tile"></span></div><span class="mini-launch">启动游戏</span></div></div></div></div>`;
}
function filteredThemes(){
  const items = themes.filter(theme => (state.provider === 'all' || theme.provider === state.provider)
    && (state.category === 'all' || state.category === theme.kind)
    && (!state.compatible || theme.compat !== 'unsupported')
    && `${theme.name} ${labels[theme.kind]} ${theme.author}`.toLowerCase().includes(state.query.toLowerCase()));
  if(state.sort === 'alpha') items.sort((a,b) => a.name.localeCompare(b.name));
  if(state.sort === 'new') items.sort((a,b) => b.order-a.order);
  return items;
}
function renderCatalog(resetScroll = false){
  const items = filteredThemes();
  const pages = Math.max(1,Math.ceil(items.length/state.pageSize));
  state.pageIndex = Math.min(state.pageIndex,pages-1);
  const visible = items.slice(state.pageIndex*state.pageSize,(state.pageIndex+1)*state.pageSize);
  if(!visible.some(theme => theme.id === state.selected)) state.selected = visible[0]?.id || null;
  const scrollTop = $('#catalog').scrollTop;
  $('#catalog').innerHTML = visible.map(theme => {
    const isInstalled = [...state.installed.values()].some(item => item.themeId === theme.id);
    return `<button class="theme-card ${theme.id === state.selected?'selected':''}" data-theme="${theme.id}" aria-label="${theme.name} · ${labels[theme.kind]}" aria-pressed="${theme.id === state.selected}">${preview(theme)}<div class="card-text"><div class="card-top"><h2>${theme.name}</h2><span class="card-kind">${labels[theme.kind]}</span></div><div class="card-author">${theme.author} · ${theme.provider === 'kde'?'KDE Store':'GitHub'}</div><div class="card-bottom">${badge(theme)}${isInstalled?'<span class="installed-mark">✓ 已安装</span>':''}</div></div></button>`;
  }).join('');
  $('#catalog').scrollTop = resetScroll ? 0 : scrollTop;
  $('#empty').hidden = items.length !== 0;
  $('#catalog').hidden = items.length === 0;
  $('#result-summary').textContent = `${items.length} 个示例主题${state.compatible ? ' · 按当前 Windows 能力筛选' : ' · 包含不可用组件'}`;
  $('#page-number').textContent = `${state.pageIndex+1} / ${pages}`;
  $('#previous').disabled = state.pageIndex === 0;
  $('#next').disabled = state.pageIndex === pages-1;
  $$('.categories button').forEach(button => {button.classList.toggle('selected',button.dataset.category === state.category);button.setAttribute('aria-pressed',String(button.dataset.category === state.category));});
  hydrateIcons($('#catalog'));
  renderDetail();
}
function renderDetail(){
  const theme = themeById(state.selected);
  $('#detail').hidden = !theme;
  if(!theme) return;
  const selected = selectedFile(theme),key = packageKey(theme,selected);
  const installed = state.installed.has(key),running = activeTask(key);
  const blocked = theme.compat === 'unsupported';
  const explanation = blocked ? theme.description : theme.compat === 'partial'
    ? '可用：配色。不可用：Plasma 面板、KWin 特效。安装前会列出可提取组件。'
    : theme.kind === 'icons' ? '图标格式可识别；安装时检查继承与包内链接，缺失图标使用内置资源。'
    : '格式可识别；安装时进一步检查文件内容。可与当前图标和系统装饰搭配。';
  $('#detail').innerHTML = `<div class="detail-header"><h2>${theme.name}</h2><div class="author">${theme.author} · ${labels[theme.kind]}</div>${badge(theme)}</div><div class="detail-scroll"><div class="detail-inner"><p class="detail-description">${theme.description}</p><dl><dt>来源</dt><dd>${theme.provider==='kde'?'KDE Store':'GitHub'} · 示例条目</dd><dt>许可</dt><dd>待真实来源提供</dd></dl><div class="compatibility-box ${theme.compat!=='ready'?'warning':''}">${explanation}</div></div></div><div class="detail-footer"><label class="file-label" for="file-select">下载文件 · ${theme.files.length} 个</label><select id="file-select" class="file-select">${theme.files.map(item=>`<option value="${item.id}" ${selected.id===item.id?'selected':''}>${item.name}</option>`).join('')}</select><div class="file-meta">v${selected.version} · ${selected.size} · ${selected.date}（示例）</div><div class="detail-actions"><button data-action="preview">预览</button><button class="primary" data-action="${running?'task':installed?'configure':'install'}" ${blocked?'disabled':''}>${blocked?'无法应用':running?'查看任务':installed?'配置使用':'安装所选文件'}</button></div>${installed?'<p class="file-meta">已安装。应用前仍使用当前外观。</p>':''}<a class="detail-link" target="_blank" rel="noopener noreferrer" href="${theme.provider==='kde'?'https://store.kde.org/':'https://github.com/PrismLauncher/Themes'}">打开${theme.provider==='kde'?' KDE Store 首页':' GitHub 来源'} ↗</a></div>`;
  hydrateIcons($('#detail'));
}
function notify(message){
  $('#toast').textContent = message;$('#toast').hidden=false;
  clearTimeout(notify.timer);notify.timer=setTimeout(()=>{$('#toast').hidden=true;},4200);
}
function go(page){
  state.page=page;
  $$('.page').forEach(node=>{node.hidden=node.id!==`page-${page}`;});
  $$('.nav').forEach(node=>{const current=node.dataset.page===page;node.classList.toggle('active',current);if(current)node.setAttribute('aria-current','page');else node.removeAttribute('aria-current');});
  if(page==='installed') renderInstalled();
  if(page==='appearance') renderAppearance();
  if(page==='tasks') renderTasks();
}
function showModal(title,content,actions=[]){
  $('#modal-title').textContent=title;$('#modal-content').innerHTML=content;
  $('#modal-actions').replaceChildren();
  actions.forEach(action=>{
    const button=document.createElement('button');button.textContent=action.label;if(action.primary)button.className='primary';
    button.addEventListener('click',()=>{if(action.run)action.run();else $('#modal').close();});$('#modal-actions').append(button);
  });
  hydrateIcons($('#modal'));if(!$('#modal').open)$('#modal').showModal();
}
function previewSelected(){
  const theme=themeById(state.selected);if(!theme)return;
  showModal(`${theme.name} · 控件示意`,`${preview(theme,selectedFile(theme))}<p>这张预览只演示配色和布局。正式版本的“本机预览”会使用独立 Qt 进程；商店作者截图将单独展示。</p>`,[{label:'返回详情'}]);
}
function requestInstall(){
  const theme=themeById(state.selected);if(!theme || theme.compat==='unsupported')return;
  const selected=selectedFile(theme);
  if(state.scenario==='offline'){notify('当前为离线缓存情境，请恢复网络后安装。');return;}
  if(theme.compat==='partial'){
    showModal('选择可安装的组件',`<p>${theme.name} 包含桌面专属组件。本次只提取当前平台可用的内容。</p><label class="component-check"><input id="extract-palette" type="checkbox" checked> 配色 · 可导入启动器</label><label class="component-check"><input type="checkbox" disabled> Plasma 面板 · 当前平台不支持</label><label class="component-check"><input type="checkbox" disabled> KWin 特效 · 当前平台不支持</label>`,[{label:'取消'},{label:'安装所选组件',primary:true,run:()=>{if(!$('#extract-palette').checked){notify('至少选择一个可用组件。');return;}$('#modal').close();startInstall(theme,selected);}}]);
  }else startInstall(theme,selected);
}
function startInstall(theme,selected){
  const key=packageKey(theme,selected);
  if(activeTask(key)||state.installed.has(key))return;
  if(state.scenario==='offline'){notify('离线情境下不能开始安装。');return;}
  const task={id:++state.sequence,key,themeId:theme.id,fileId:selected.id,name:theme.name,file:selected.name,progress:0,status:'running',stage:'正在解析下载文件',fail:state.scenario==='failure'};
  state.tasks.unshift(task);renderTaskState(true);notify('已添加模拟安装任务，可在下载任务中查看或取消。');
  task.timer=setInterval(()=>{
    if(task.status!=='running'){clearInterval(task.timer);return;}
    task.progress=Math.min(100,task.progress+8);
    task.stage=task.progress<48?'正在下载文件':task.progress<64?'正在校验文件':task.progress<80?'正在解压并检查组件':task.progress<100?'正在发布主题包':'已安装，可配置使用';
    if(task.fail&&task.progress>=56){task.status='failed';task.stage='示例下载失败，原有主题保持可用';clearInterval(task.timer);}
    else if(task.progress===100){
      task.status='installed';clearInterval(task.timer);
      state.installed.set(key,{key,themeId:theme.id,fileId:selected.id,name:`${theme.name}${theme.files.length>1?selected.id==='dark'?' · Dark':' · Light':''}`,provider:theme.provider==='kde'?'KDE Store':'GitHub',palette:selected.palette,components:[...selected.components],version:selected.version});
      notify(`${theme.name} 已安装，当前生效外观尚未改变。`);renderCatalog();
    }
    renderTaskState(task.status!=='running');
  },350);
}
function cancelTask(id){
  const task=state.tasks.find(item=>item.id===id);if(!task||task.status!=='running')return;
  clearInterval(task.timer);task.status='cancelled';task.stage='已取消，未发布主题包';renderTaskState(true);notify('安装任务已取消。');
}
function renderTaskState(terminalChange=false){
  const running=state.tasks.filter(task=>task.status==='running').length;
  $('#task-count').textContent=String(running);$('#installed-count').textContent=String(state.installed.size);
  $('#footer-tasks').textContent=running?`${running} 项模拟安装正在进行`:'没有进行中的任务';
  renderTasks();if(terminalChange){renderDetail();if(state.page==='installed')renderInstalled();}
}
function renderTasks(){
  if(!state.tasks.length){
    $('#task-list').innerHTML=`<div class="empty">${icon('download')}<h2>还没有下载任务</h2><p>在在线主题中选择一个文件开始安装。</p><button data-go="discover">浏览在线主题</button></div>`;
  }else{
    $('#task-list .empty')?.remove();
    state.tasks.forEach(task=>{
      let row=$(`#task-${task.id}`);
      if(!row){
        row=document.createElement('article');row.id=`task-${task.id}`;row.className='task-item';
        row.innerHTML=`<div class="task-title"><h2>${task.name}</h2><span class="badge"></span></div><p>${task.file} · 本地模拟任务</p><progress max="100" aria-label="${task.name} 安装进度"></progress><div class="task-bottom"><span class="task-stage"></span><div class="task-actions"></div></div>`;
        $('#task-list').prepend(row);
      }
      row.querySelector('progress').value=task.progress;
      row.querySelector('.task-stage').textContent=task.stage;
      const statusKey=`${task.status}:${Boolean(activeTask(task.key))}`;
      if(row.dataset.status!==statusKey){
        row.dataset.status=statusKey;
        row.querySelector('.badge').textContent={running:'安装中',failed:'失败',cancelled:'已取消',installed:'已安装'}[task.status];
        row.querySelector('.badge').className=`badge ${task.status==='failed'?'unsupported':task.status==='running'?'neutral':''}`;
        row.querySelector('.task-stage').classList.toggle('task-error',task.status==='failed');
        row.querySelector('.task-actions').innerHTML=task.status==='running'?`<button data-cancel="${task.id}">取消</button>`:task.status==='installed'?`<button data-configure="${task.key}">配置使用</button>`:`<button data-retry="${task.id}" ${activeTask(task.key)?'disabled':''}>重试</button>`;
      }
    });
  }
  hydrateIcons($('#task-list'));
}
function packageUsed(key,config){return config && config.mode==='independent' && [config.palette,config.icons,config.skin].includes(key);}
function renderInstalled(){
  $('#installed-list').innerHTML=[...state.installed.values()].map(item=>{
    const current=packageUsed(item.key,state.effective),pending=packageUsed(item.key,state.pending);
    const protectedPackage=item.key==='builtin'||current||pending;
    return `<article class="installed-row"><div class="installed-symbol">${icon(item.components.includes('icons')&&!item.components.includes('palette')?'image':'palette')}</div><div><h2>${item.name}</h2><p>${item.provider} · ${item.version} · ${item.components.map(c=>labels[c]).join(' / ')}</p></div><div class="row-actions"><span class="badge ${current?'':'neutral'}">${current?'正在使用':pending?'待重启使用':'已安装'}</span><button data-configure="${item.key}">配置使用</button><button data-remove="${item.key}" ${protectedPackage?'disabled':''} title="${protectedPackage?'内置、正在使用或待生效的组件不能移除':'移除此示例包'}">移除</button></div></article>`;
  }).join('');hydrateIcons($('#installed-list'));
}
function configure(key){
  const item=state.installed.get(key);if(!item)return;
  state.draft.mode='independent';
  item.components.forEach(component=>{if(component==='palette'||component==='icons'||component==='skin')state.draft[component]=key;});
  go('appearance');
}
function nameFor(key,type){if(key==='builtin')return type==='icons'?'Luna 图标':'Luna Light';return state.installed.get(key)?.name||'默认';}
function configName(config){return config.mode==='system'?'跟随系统':nameFor(config.palette,'palette');}
function renderHeader(){
  $('#effective-name').textContent=configName(state.effective);
  $('#effective-meta').textContent=state.effective.mode==='system'?'由操作系统提供外观':`${state.effective.style} · ${nameFor(state.effective.icons,'icons')} · ${state.effective.decoration==='system'?'系统装饰':'Luna 装饰'}`;
  $('#pending-banner').hidden=!state.pending;
  if(state.pending)$('#pending-text').textContent=`待重启生效：${configName(state.pending)} · ${state.pending.decoration==='system'?'系统装饰':'Luna 装饰'}`;
}
function renderAppearance(){
  ['palette','icons'].forEach(component=>{
    $(`#${component}`).innerHTML=[...state.installed.values()].filter(item=>item.components.includes(component)).map(item=>`<option value="${item.key}">${nameFor(item.key,component)}</option>`).join('');
  });
  $('#skin').innerHTML='<option value="none">无额外皮肤</option><option value="arc">Luna Arc · 示例内置</option>'+[...state.installed.values()].filter(item=>item.components.includes('skin')).map(item=>`<option value="${item.key}">${item.name}</option>`).join('');
  ['style','skin','palette','icons','decoration'].forEach(key=>{$(`#${key}`).value=state.draft[key];});
  $$('input[name=mode]').forEach(input=>{input.checked=input.value===state.draft.mode;});
  updateCombinationPreview();
}
function needsRestart(config){
  return ['mode','style','skin','decoration'].some(key=>config[key]!==state.effective[key]);
}
function changes(config){
  const names={mode:'外观模式',style:'控件风格',skin:'控件皮肤',palette:'配色',icons:'图标',decoration:'窗口装饰'};
  return Object.keys(names).filter(key=>config[key]!==state.effective[key]).map(key=>names[key]);
}
function updateCombinationPreview(){
  $('#component-fields').disabled=state.draft.mode==='system';
  const palette=state.draft.mode==='system'?'light':state.installed.get(state.draft.palette)?.palette||'light';
  $('#combination-sample').innerHTML=preview({palette},null,state.draft.mode==='system'?'system':state.draft.decoration,state.draft.mode==='system'?'none':state.draft.skin);
  const different=changes(state.draft);
  $('#impact').textContent=!different.length?'组合与当前生效外观一致。':needsRestart(state.draft)?'此更改涉及外观模式、控件皮肤或窗口装饰。保存后将在下次启动生效。':'仅更换受控配色或图标，可直接应用。';
  $('#change-list').textContent=different.length?`将更改：${different.join('、')}`:'';
  $('#apply').textContent=needsRestart(state.draft)?'保存，待重启生效':'应用组合';
  $('#apply').disabled=!different.length&&!state.pending;
}
function applyCombination(){
  if(needsRestart(state.draft)){
    state.pending={...state.draft};notify('已保存待生效组合，当前界面仍使用原组合。');
  }else{
    state.effective={...state.draft};state.pending=null;notify('组合已在原型中生效。');
  }
  state.revision++;renderHeader();updateCombinationPreview();
}
function renderNetwork(){
  const offline=state.scenario==='offline';
  $('#network-notice').hidden=!offline;
  $('#network-notice').textContent='离线浏览 · 正在显示本地示例缓存。真实版本会显示缓存时间；恢复连接后可刷新和安装。';
  $('#footer-network').textContent=`本地演示 · ${offline?'离线缓存情境':state.scenario==='failure'?'安装失败情境':'正常网络情境'}`;
}
function resetQuery(){state.queryGeneration++;state.pageIndex=0;$('#refresh').disabled=false;$('#refresh').innerHTML=`${icon('refresh-cw')}刷新`;hydrateIcons($('#refresh'));renderNetwork();renderCatalog(true);}
$('#provider').addEventListener('change',event=>{state.provider=event.target.value;resetQuery();});
$('#search').addEventListener('input',event=>{state.query=event.target.value;resetQuery();});
$('#compatible').addEventListener('change',event=>{state.compatible=event.target.checked;resetQuery();});
$('#sort').addEventListener('change',event=>{state.sort=event.target.value;resetQuery();});
$('#scenario').addEventListener('change',event=>{state.scenario=event.target.value;state.queryGeneration++;$('#refresh').disabled=false;$('#refresh').innerHTML=`${icon('refresh-cw')}刷新`;hydrateIcons($('#refresh'));renderNetwork();});
$('#previous').addEventListener('click',()=>{if(state.pageIndex>0){state.pageIndex--;renderCatalog(true);}});
$('#next').addEventListener('click',()=>{if((state.pageIndex+1)*state.pageSize<filteredThemes().length){state.pageIndex++;renderCatalog(true);}});
$('#refresh').addEventListener('click',()=>{
  if(state.scenario==='offline'){notify('离线刷新未成功，保留当前目录和浏览位置。');return;}
  const generation=++state.queryGeneration;
  $('#refresh').disabled=true;$('#refresh').textContent='刷新中…';
  setTimeout(()=>{if(generation!==state.queryGeneration)return;$('#refresh').disabled=false;$('#refresh').innerHTML=`${icon('refresh-cw')}刷新`;hydrateIcons($('#refresh'));notify('示例目录已刷新，选中项与浏览位置保持不变。');},700);
});
$('#clear-filters').addEventListener('click',()=>{state.query='';state.category='all';state.compatible=true;state.provider='kde';$('#search').value='';$('#compatible').checked=true;$('#provider').value='kde';resetQuery();});
$('#detail').addEventListener('change',event=>{if(event.target.id==='file-select'){state.selectedFiles.set(state.selected,event.target.value);renderDetail();}});
$('#detail').addEventListener('click',event=>{
  const button=event.target.closest('[data-action]');if(!button)return;
  const theme=themeById(state.selected);if(!theme)return;
  if(button.dataset.action==='preview')previewSelected();
  if(button.dataset.action==='install')requestInstall();
  if(button.dataset.action==='configure')configure(packageKey(theme,selectedFile(theme)));
  if(button.dataset.action==='task')go('tasks');
});
document.addEventListener('click',event=>{
  const button=event.target.closest('button');if(!button || button.disabled)return;
  if(button.dataset.page)go(button.dataset.page);
  if(button.dataset.go)go(button.dataset.go);
  if(button.dataset.category){state.category=button.dataset.category;resetQuery();}
  if(button.dataset.theme){state.selected=button.dataset.theme;$$('.theme-card').forEach(card=>{const current=card.dataset.theme===state.selected;card.classList.toggle('selected',current);card.setAttribute('aria-pressed',String(current));});renderDetail();}
  if(button.dataset.cancel)cancelTask(Number(button.dataset.cancel));
  if(button.dataset.retry){const task=state.tasks.find(item=>item.id===Number(button.dataset.retry));if(task){const theme=themeById(task.themeId);startInstall(theme,theme.files.find(item=>item.id===task.fileId));}}
  if(button.dataset.configure)configure(button.dataset.configure);
  if(button.dataset.remove){
    const key=button.dataset.remove,item=state.installed.get(key);if(!item||key==='builtin'||packageUsed(key,state.effective)||packageUsed(key,state.pending))return;
    showModal('移除已安装主题',`<p>将从示例目录中移除 ${escapeHtml(item.name)}，可以重新安装。</p>`,[{label:'取消'},{label:'移除',run:()=>{state.installed.delete(key);['palette','icons','skin'].forEach(component=>{if(state.draft[component]===key)state.draft[component]=component==='skin'?'none':'builtin';});$('#modal').close();renderInstalled();renderCatalog();$('#installed-count').textContent=String(state.installed.size);}}]);
  }
});
$$('input[name=mode]').forEach(input=>input.addEventListener('change',()=>{state.draft.mode=input.value;updateCombinationPreview();}));
['style','skin','palette','icons','decoration'].forEach(key=>$(`#${key}`).addEventListener('change',event=>{state.draft[key]=event.target.value;updateCombinationPreview();}));
$('#apply').addEventListener('click',applyCombination);
$('#undo-pending').addEventListener('click',()=>{state.pending=null;state.draft={...state.effective};state.revision++;renderHeader();renderAppearance();notify('已撤销待生效更改。');});
$('#restart').addEventListener('click',()=>{if(!state.pending)return;state.effective={...state.pending};state.pending=null;state.draft={...state.effective};state.revision++;renderHeader();renderAppearance();notify('模拟启动完成，新组合已标为生效。');});
$('#preview-combination').addEventListener('click',()=>showModal('外观组合 · 控件示意',$('#combination-sample').innerHTML+'<p>这里演示控件、配色与标题栏的组合关系。实际 Qt 样式、阴影及窗口行为需要原生预览验证。</p>',[{label:'关闭预览'}]));
$('#modal-close').addEventListener('click',()=>$('#modal').close());
$('#sources').addEventListener('click',()=>showModal('主题来源',`<div class="source-row">${icon('globe')}<div><strong>KDE Store</strong><small>api.kde-look.org / OCS</small></div><span class="badge">默认来源</span></div><div class="source-row">${icon('globe')}<div><strong>OpenDesktop · Kvantum</strong><small>api.opendesktop.org / OCS · 分类 123</small></div><span class="badge neutral">按类别使用</span></div><div class="source-row">${icon('archive')}<div><strong>GitHub · PrismLauncher/Themes</strong><small>现有发布来源继续保留</small></div></div><p>正式页面将提供启用、添加和移除来源。这个原型只展示来源结构，没有连接远程目录。</p>`,[{label:'关闭'}]));
$('#import-local').addEventListener('click',()=>showModal('本地主题导入',`<p>正式功能将直接调用系统文件选择器，支持 .colors、图标压缩包和 Luna 主题包；导入进入与在线安装相同的扫描与发布流程。</p><p>当前为页面原型，未接入文件安装服务。可以通过在线主题中的示例条目体验安装后的管理流程。</p>`,[{label:'关闭'},{label:'浏览示例主题',primary:true,run:()=>{$('#modal').close();go('discover');}}]));
hydrateIcons();renderCatalog();renderHeader();renderInstalled();renderTasks();renderAppearance();renderNetwork();
