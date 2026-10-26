'use strict';
const byId = (id) => document.getElementById(id);
const esc = (value) => String(value).replace(/[&<>"']/g, (ch) => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[ch]));
const skyRewriteHistoryPageSize = 100;
const skyRewriteHistoryMaxRows = 2000;
const requestSkyRewriteHistoryPage = (offset = 0) => window.parent.postMessage({ channel: 'skyrewrite-dashboard', type: 'request-history', offset, limit: skyRewriteHistoryPageSize }, '*');
const state = {
  activePage: 'skyrewrite', settings: {},
  skycollectionItems: [], skycollectionLocations: [], skycollectionRarity: 'all', skycollectionPlugin: 'all',
  skyAchievementsPickerOptions: {}, skyAchievementsFilters: {},
  skyrewriteTimer: 0, skyrewriteVisible: false, skyrewriteTab: 'history', skyrewriteHistoryNextOffset: 0, skyrewriteHistoryTotal: 0, skyrewriteHistoryHasMore: false, skyrewriteHistoryLoading: false, skyrewriteHistoryReloadPending: false, skyrewriteTheftFilter: 'all', skyrewriteTheftSearch: '', skyrewriteEvents: [],
  skyrewriteCurrentDay: null, skyrewriteRecorder: null, skyrewriteRegionalBanditKills: null,
  skyrewriteRegionalBanditStates: [], skyrewriteRegionalBanditHoldStates: [], skyrewriteRegionalCreatureStates: [], skyrewriteRegionalFavorStates: [],
  skyrewriteReputation: null, skyrewriteGameBounties: [], skyrewriteHold: 'eastmarch', skyrewriteFilter: 'all',
  skyrewriteLocationFilter: 'all', skyrewritePluginFilter: 'all', skyrewriteTimeRange: 'all', skyrewriteSearch: '',
  skyrewriteMemoryFilter: 'all', skyrewriteMemorySearch: '', skyrewritePressureFilter: 'all', skyrewritePressureSearch: '',
  skyrewritePressureLocation: 'all', skyrewritePressurePlugin: 'all', skyrewritePressureSort: 'pressure', skyrewritePressureExpanded: new Set(),
  skyrewriteActorFilter: 'all', skyrewriteActorSearch: '', skyrewriteActorSort: 'kills', skyrewriteActorGrouping: 'type',
  skyrewriteActorLocation: 'all', skyrewriteActorPlugin: 'all', skyrewriteActorLocationOptions: [], skyrewriteActorPluginOptions: [],
  skyrewriteActorExpanded: new Set()
};
function ensureSkyRewriteHistoryLoaded() {
  if (state.skyrewriteHistoryLoading || state.skyrewriteTab === 'history') return;
  if (!state.skyrewriteHistoryNextOffset) {
    state.skyrewriteHistoryLoading = true;
    requestSkyRewriteHistoryPage(0);
  } else if (state.skyrewriteHistoryHasMore && state.skyrewriteHistoryNextOffset < skyRewriteHistoryMaxRows) {
    state.skyrewriteHistoryLoading = true;
    requestSkyRewriteHistoryPage(state.skyrewriteHistoryNextOffset);
  }
}
function reloadSkyRewriteHistory() {
  if (state.skyrewriteHistoryLoading) {
    state.skyrewriteHistoryReloadPending = true;
    return;
  }
  state.skyrewriteHistoryReloadPending = false;
  state.skyrewriteHistoryNextOffset = 0;
  state.skyrewriteHistoryTotal = 0;
  state.skyrewriteHistoryHasMore = false;
  state.skyrewriteHistoryLoading = true;
  requestSkyRewriteHistoryPage(0);
}
const invoke = (name, value = '') => window.parent.postMessage({ channel: 'skyrewrite-dashboard', type: 'action', name, value }, '*');
const defaultSkyRewriteSettings = {
  historyRefreshInterval: 3000, showLocationSummary: true, showInventoryEvents: true, journalRetentionEvents: 2048,
  localEchoEnabled: false, localEchoKillThreshold: 3, localEchoLookbackDays: 7, localEchoCooldownDays: 7,
  localEchoShowNotification: true, adaptiveGuardResponse: false, localEchoTemporaryBounty: false,
  echoBountyVictimMode: 'nonhostile', localEchoTemporaryBountyAmount: 25, localEchoTemporaryBountyDays: 1,
  localEchoTemporaryBountyScaleWithPressure: false, theftBountyEnabled: false, theftBountyAmount: 25,
  theftBountyThreshold: 3, dispatchGuardOnBounty: true
};
const skyRewriteSettingGroups = [
  { name: 'Journal & display', settings: [
    { id: 'historyRefreshInterval', label: 'Live history update delay', type: 'number', min: 250, max: 30000, step: 1, suffix: 'ms', description: 'Wait this long after the latest recorded action, then refresh the visible history once.' },
    { id: 'showLocationSummary', label: 'Show observed activity by location', type: 'boolean', description: 'Show or hide the activity summary on the overview.' },
    { id: 'showInventoryEvents', label: 'Include inventory transfers in history', type: 'boolean', description: 'Show item movement events in the journal.' },
    { id: 'journalRetentionEvents', label: 'Journal retention', type: 'number', min: 512, max: 2048, step: 256, suffix: 'events', description: 'Older rows are pruned first; saved lifetime totals remain.' }
  ] },
  { name: 'Local Echo', settings: [
    { id: 'localEchoEnabled', label: 'Enable Local Echo on revisit', type: 'boolean', description: 'Record an Echo when recent weighted activity reaches its threshold.' },
    { id: 'localEchoKillThreshold', label: 'Local Echo pressure threshold', type: 'number', min: 1, max: 20, step: 1, description: 'Recent activity needed to record an Echo.' },
    { id: 'localEchoLookbackDays', label: 'Local Echo lookback', type: 'number', min: 0.5, max: 60, step: 0.5, suffix: 'in-game days', description: 'How far back recent activity contributes to pressure.' },
    { id: 'localEchoCooldownDays', label: 'Local Echo cooldown', type: 'number', min: 0.5, max: 60, step: 0.5, suffix: 'in-game days', description: 'Minimum time before the same location can record another Echo.' },
    { id: 'localEchoShowNotification', label: 'Show an in-game Echo notice', type: 'boolean', description: 'Show a short HUD message when an Echo is recorded.' }
  ] },
  { name: 'Hold response', settings: [
    { id: 'adaptiveGuardResponse', label: 'Adaptive guard confidence response', type: 'boolean', description: 'Eligible guards become one confidence step braver for one in-game day after repeated hostile losses.' },
    { id: 'localEchoTemporaryBounty', label: 'Add a temporary hold bounty on Echo', type: 'boolean', description: 'Add a temporary nonviolent bounty when an eligible Echo fires.' },
    { id: 'echoBountyVictimMode', label: 'Echo bounty eligible victim type', type: 'select', options: [['nonhostile', 'Non-hostile victims'], ['hostile', 'Hostile victims'], ['both', 'Both']], description: 'Choose which saved victim context qualifies an Echo bounty.' },
    { id: 'localEchoTemporaryBountyAmount', label: 'Temporary hold bounty amount', type: 'number', min: 5, max: 100, step: 5, suffix: 'septims', description: 'Nonviolent bounty added by an Echo.' },
    { id: 'localEchoTemporaryBountyDays', label: 'Temporary hold bounty duration', type: 'number', min: 0.5, max: 7, step: 0.5, suffix: 'in-game days', description: 'How long the Echo bounty remains before a safe expiry attempt.' },
    { id: 'localEchoTemporaryBountyScaleWithPressure', label: 'Scale bounty with local pressure', type: 'boolean', description: 'Scale the configured amount by pressure, rounded to 5 septims and capped at 100.' },
    { id: 'theftBountyEnabled', label: 'Add a bounty after repeated thefts', type: 'boolean', description: 'Let repeated thefts at one location trigger a nonviolent hold bounty.' },
    { id: 'theftBountyAmount', label: 'Theft bounty amount', type: 'number', min: 5, max: 100, step: 5, suffix: 'septims', description: 'Amount attempted after the theft threshold is reached.' },
    { id: 'theftBountyThreshold', label: 'Theft events before bounty', type: 'number', min: 1, max: 100, step: 1, description: 'Theft events in one location and lookback window required for a bounty attempt.' },
    { id: 'dispatchGuardOnBounty', label: 'Dispatch a matching hold guard', type: 'boolean', description: 'Ask SkyRewrite to dispatch a guard when a hold bounty is applied.' }
  ] }
];
try {
  const savedSkyRewriteSettings = JSON.parse(localStorage.getItem('skyrewrite-settings-v1') || '{}');
  state.settings = { ...defaultSkyRewriteSettings, ...(savedSkyRewriteSettings && typeof savedSkyRewriteSettings === 'object' ? savedSkyRewriteSettings : {}) };
} catch { state.settings = { ...defaultSkyRewriteSettings }; }
function eventPluginOwner(event, kind = 'form') {
    if (kind === 'base') return String(event?.basePlugin || '').trim();
    if (kind === 'location') return String(event?.locationPlugin || '').trim();
    return String(event?.formPlugin || '').trim();
  }


  const skyRewriteEventCategoryKinds = {
    all: null,
    world: new Set(['location', 'discovery', 'death']),
    quest: new Set(['quest', 'reputation']),
    inventory: new Set(['inventory']),
    actions: new Set(['lockpick', 'consumption', 'interaction', 'forgery', 'spell', 'shout', 'perk', 'discovery', 'equipment', 'skill', 'level', 'read', 'ingredientEffect', 'follower', 'crafting', 'disenchantment', 'brawl']),
    echo: new Set(['echo'])
  };

  function isInfrastructureQuestHistoryEvent(event) {
    if (event?.kind !== 'quest') return false;
    const editorID = String(event.identity || '').toLocaleLowerCase();
    const questLabel = String(event.subject || '').toLocaleLowerCase();
    const markers = `${editorID} ${questLabel}`;
    return /mcm|detectquest|trackevent|handlingchange|easeburden|configuration|configquest|settings?quest|widgetquest|overlayquest|frameworkquest/.test(markers) ||
      /menu/.test(editorID);
  }

  function spellTomeNameKey(value) {
    const title = String(value || '').trim();
    if (!/^spell tome\s*:/i.test(title)) return '';
    return title.replace(/^spell tome\s*:/i, '').trim().replace(/\s+/g, ' ').toLocaleLowerCase();
  }

  function spellNameKey(value) {
    const name = String(value || '').trim();
    if (!name || /^unknown(?:\s+spell)?$/i.test(name) || /^spell tome\s*:/i.test(name)) return '';
    return name.replace(/\s+/g, ' ').toLocaleLowerCase();
  }

  function sameHistoryDay(left, right) {
    if (left?.day == null || left.day === '' || right?.day == null || right.day === '') return false;
    const leftDay = Number(left?.day);
    const rightDay = Number(right?.day);
    return Number.isFinite(leftDay) && Number.isFinite(rightDay) && leftDay.toFixed(1) === rightDay.toFixed(1);
  }

  function relatedSpellTomeReadEvent(event) {
    const history = state.skyrewriteEvents || [];
    const eventIndex = history.indexOf(event);
    const directName = spellNameKey(event?.subject);
    let closestMatch = null;
    let closestMatchDistance = Infinity;
    let closestTome = null;
    let closestTomeDistance = Infinity;
    history.forEach((candidate, index) => {
      if (candidate?.kind !== 'read' || !sameHistoryDay(candidate, event)) return;
      const tomeName = spellTomeNameKey(candidate.subject);
      if (!tomeName) return;
      const distance = eventIndex < 0 ? index : Math.abs(index - eventIndex);
      if (distance < closestTomeDistance) {
        closestTome = candidate;
        closestTomeDistance = distance;
      }
      if (directName && tomeName !== directName && !tomeName.startsWith(`${directName} |`) && !directName.startsWith(`${tomeName} |`)) return;
      if (distance < closestMatchDistance) {
        closestMatch = candidate;
        closestMatchDistance = distance;
      }
    });
    return closestMatch || (!directName ? closestTome : null);
  }

  function learnedSpellNameFromHistory(event) {
    const directName = spellNameKey(event?.subject);
    const relatedRead = relatedSpellTomeReadEvent(event);
    if (relatedRead) return String(relatedRead.subject).replace(/^spell tome\s*:/i, '').trim();
    return directName ? String(event.subject).trim() : '';
  }

  function cleanSpellTomePrefix(value) {
    return String(value || '').trim().replace(/^(?:spell\s+tome\s*:\s*)+/i, '').trim();
  }

  function isSpellTomeEvent(event) {
    return event?.spellTome === true || Boolean(spellTomeNameKey(event?.subject));
  }

  function spellTomeMatchesLearnedEvent(tomeEvent, learnedEvent) {
    if (!sameHistoryDay(tomeEvent, learnedEvent)) return false;
    const tomeID = Number(tomeEvent?.taughtSpellFormID) >>> 0;
    const learnedID = Number(learnedEvent?.formID) >>> 0;
    if (tomeID && learnedID && tomeID === learnedID) return true;
    const relatedRead = relatedSpellTomeReadEvent(learnedEvent);
    if (relatedRead && (Number(relatedRead.formID) >>> 0) === (Number(tomeEvent?.formID) >>> 0)) return true;
    const tomeName = spellTomeNameKey(tomeEvent?.subject);
    const learnedName = spellNameKey(learnedSpellNameFromHistory(learnedEvent));
    return Boolean(tomeName && learnedName && tomeName === learnedName);
  }

  function matchesSkyRewriteHistoryEvent(event, latestDay, settings) {
    if (isInfrastructureQuestHistoryEvent(event)) return false;
    if (isSpellTomeEvent(event) && ['read', 'equipment', 'inventory'].includes(event.kind)) {
      const wasLearnedHere = (state.skyrewriteEvents || []).some((candidate) =>
        candidate?.kind === 'spell' && spellTomeMatchesLearnedEvent(event, candidate));
      if (wasLearnedHere) return false;
    }
    if (!settings.showInventoryEvents && event.kind === 'inventory') return false;
    const filter = state.skyrewriteFilter;
    const allowedKinds = skyRewriteEventCategoryKinds[filter] || null;
    if (allowedKinds && !allowedKinds.has(event.kind)) return false;
    if (state.skyrewriteLocationFilter !== 'all' && (event.location || 'Location unavailable') !== state.skyrewriteLocationFilter) return false;
    if (state.skyrewriteTimeRange !== 'all' && (event.day == null || event.day === '' || !Number.isFinite(Number(event.day)) || Number(event.day) < latestDay - Number(state.skyrewriteTimeRange))) return false;
    const formOwner = eventPluginOwner(event);
    const baseOwner = eventPluginOwner(event, 'base');
    const locationOwner = eventPluginOwner(event, 'location');
    if (state.skyrewritePluginFilter !== 'all' && formOwner !== state.skyrewritePluginFilter && baseOwner !== state.skyrewritePluginFilter && locationOwner !== state.skyrewritePluginFilter) return false;
    const formIDText = Number(event.formID || 0).toString(16).toUpperCase().padStart(8, '0');
    const baseFormIDText = Number(event.baseFormID || 0).toString(16).toUpperCase().padStart(8, '0');
    const searchNeedle = state.skyrewriteSearch.trim().toLocaleLowerCase();
    return !searchNeedle || [event.subject, event.identity, event.detail, event.location, formOwner, baseOwner, locationOwner, `formid ${formIDText}`, `base formid ${baseFormIDText}`, event.day == null ? '' : `day ${event.day}`]
      .some((value) => String(value || '').toLocaleLowerCase().includes(searchNeedle));
  }

  function renderSkyRewriteHistoryList(items, settings) {
    const host = byId('skyrewrite-history');
    const count = byId('skyrewrite-event-count');
    if (!host || !count) return;
    if (!settings.showInventoryEvents && state.skyrewriteFilter === 'inventory') state.skyrewriteFilter = 'all';
    const latestDay = items.reduce((latest, event) => event.day != null && event.day !== '' && Number.isFinite(Number(event.day)) ? Math.max(latest, Number(event.day)) : latest, -Infinity);
    const matchingItems = items.filter((event) => matchesSkyRewriteHistoryEvent(event, latestDay, settings));
    const visibleItems = matchingItems.slice(0, skyRewriteHistoryMaxRows);
    const recorder = state.skyrewriteRecorder;
    const totalRows = Math.min(state.skyrewriteHistoryTotal || items.length, skyRewriteHistoryMaxRows);
    const historySummary = `${matchingItems.length} matching in loaded records · ${items.length.toLocaleString()} loaded · ${totalRows.toLocaleString()} available`;
    count.textContent = `${historySummary}${recorder ? ` · retained kills ${recorder.retainedKills} · Local Echoes ${recorder.retainedEchoes} · recorder ${recorder.installed ? 'listening' : 'offline'} · ${recorder.playerKills} player kills captured this session` : ''}`;
    document.querySelectorAll('[data-skyrewrite-filter]').forEach((button) => {
      const active = button.dataset.skyrewriteFilter === state.skyrewriteFilter;
      button.classList.toggle('active', active);
      button.setAttribute('aria-pressed', active ? 'true' : 'false');
    });
    host.replaceChildren();
    if (!visibleItems.length) {
      const empty = document.createElement('div'); empty.className = 'empty';
      empty.textContent = items.length ? 'No events match these filters.' : recorder
        ? `No tracked events in this save. Recorder is ${recorder.installed ? 'listening' : 'offline'}; ${recorder.playerKills} player kills were captured this session.`
        : 'No tracked events in this save yet. Recorder status is unavailable; update the SkyRewrite plugin to see capture diagnostics.';
      host.appendChild(empty);
    }
    const shownItems = visibleItems;
    for (const event of shownItems) {
      const row = document.createElement('article'); row.className = 'skyrewrite-event';
      const day = document.createElement('span'); day.className = 'skyrewrite-event-day';
      day.textContent = event.day != null && Number.isFinite(Number(event.day)) ? `Day ${Number(event.day).toFixed(1)}` : 'Day unknown';
      const copy = document.createElement('div');
      const inventoryDetail = event.kind === 'inventory' && event.detail?.startsWith('Lost') ? event.detail.replace(/^Lost\b/, 'Transferred out') : event.kind === 'inventory' && event.detail?.startsWith('Gained') ? event.detail.replace(/^Gained\b/, 'Transferred in') : event.detail;
      const isTheft = event.kind === 'inventory' && (event.isTheft === true || Number(event.eventType) === 10 || String(event.identity || '') === 'SkyRewrite/Theft/v1');
      const stolenQuantity = Number(String(event.detail || '').match(/^Stole\s+(\d+)/i)?.[1]) || Number(event.stage) || 1;
      const itemFormID = (Number(event.formID || 0) >>> 0).toString(16).toUpperCase().padStart(8, '0');
      const stolenTitle = event.theftSourceKind === 'pickpocket'
        ? `Pickpocketed ${event.sourceName || 'Unknown NPC'}`
        : `Stole ${event.subject || 'Unknown item'} (FormID ${itemFormID})${stolenQuantity > 1 ? ` ×${stolenQuantity}` : ''}`;
      const resolutionLabel = String(event.detail || '').includes('manually removed') ? 'Temporary hold bounty removed manually' : String(event.detail || '').includes('verification failed') ? 'Temporary hold bounty removal verification failed' : String(event.detail || '').includes('skipped') ? 'Temporary hold bounty expiry skipped' : 'Temporary hold bounty expired';
      const title = document.createElement('strong'); title.textContent = event.kind === 'echo' && Number(event.eventType) === 8 ? `Local guards adapted · ${event.subject || 'Unknown guard'}` : event.kind === 'echo' && Number(event.eventType) === 9 ? `Guard response expired · ${event.subject || 'Unknown guard'}` : event.kind === 'echo' && Number(event.eventType) === 6 ? `Temporary hold bounty applied · ${event.subject || 'Unknown location'}` : event.kind === 'echo' && Number(event.eventType) === 7 ? `${resolutionLabel} · ${event.subject || 'Unknown location'}` : event.kind === 'echo' && Number(event.eventType) === 12 ? `Local Echo cooldown · ${event.subject || 'Unknown location'}` : event.kind === 'echo' && Number(event.eventType) === 13 ? `${String(event.detail || '').includes('spawned') ? 'Hold guard dispatched' : 'Hold guard dispatch failed'} · ${event.subject || 'Unknown location'}` : event.kind === 'echo' && Number(event.eventType) === 11 ? `Theft bounty trigger skipped · ${event.subject || 'Unknown location'}` : event.kind === 'echo' ? `Local Echo · ${event.subject || 'Unknown location'}` : event.kind === 'bounty' ? `Skyrim bounty cleared · ${event.subject || 'Unknown hold'}` : event.kind === 'death' ? (event.detail === 'Player killed this actor' ? `Killed ${event.subject || 'unknown actor'}` : `Player death · ${event.detail || 'cause unknown'}`) : event.kind === 'location' ? `${event.detail || 'Location changed'} · ${event.subject || 'Unknown location'}` : event.kind === 'inventory' ? `${inventoryDetail || 'Inventory transfer'} · ${event.subject || 'Unknown item'}` : event.kind === 'reputation' ? `Reputation gift received · ${event.subject || 'Unknown gift'}` : `${event.subject || event.identity || `Quest FormID ${Number(event.formID || 0).toString(16).toUpperCase().padStart(8, '0')}`} · stage ${Number(event.stage) || 0}`;
      if (isTheft) title.textContent = event.isGold ? (inventoryDetail || 'Gold stolen') : stolenTitle;
      if (event.kind === 'inventory' && !isTheft) {
        const quantity = !event.isGold && Number(event.stage) > 1 ? ` ×${Number(event.stage)}` : '';
        title.textContent = event.isGold ? (inventoryDetail || 'Gold inventory action') : `${inventoryDetail || 'Inventory change'} · ${event.subject || 'Unknown item'}${quantity}`;
      }
      if (event.kind === 'echo' && Number(event.eventType) === 11) title.textContent = `Theft bounty trigger skipped · ${event.subject || 'Unknown location'}`;
      if (event.kind === 'shopTheft') title.textContent = `Shop robbed · ${event.subject || 'Local merchant'}`;
      if (event.kind === 'lockpick') title.textContent = `Lockpicked ${event.subject || 'unknown container'}`;
      if (event.kind === 'consumption') title.textContent = `Consumed ${event.subject || 'unknown item'}`;
      if (event.kind === 'interaction') title.textContent = `${event.detail || 'Spoke with NPC'} · ${event.subject || 'Unknown NPC'}`;
      if (event.kind === 'forgery') title.textContent = `Forged records · ${event.location || event.subject || 'Unknown location'}`;
      if (event.kind === 'spell') title.textContent = `Learned Spell ${cleanSpellTomePrefix(learnedSpellNameFromHistory(event)) || 'Unknown'}`;
      if (event.kind === 'shout') title.textContent = `Learned Shout · ${event.subject || 'Unknown shout'}`;
      if (event.kind === 'perk') title.textContent = `Unlocked Perk · ${event.subject || 'Unknown perk'}`;
      if (event.kind === 'discovery') title.textContent = `Discovered Location · ${event.subject || 'Unknown location'}`;
      if (event.kind === 'equipment') title.textContent = `${Number(event.eventType) === 30 ? 'Equipped' : 'Unequipped'} ${event.subject || 'Unknown item'} (FormID ${itemFormID})`;
      if (event.kind === 'skill') title.textContent = `Skill ${event.subject || 'Unknown skill'} increased by ${Number(event.stage) || 1}`;
      if (event.kind === 'level') title.textContent = `Level increased by ${Number(event.stage) || 1}`;
      if (event.kind === 'read') title.textContent = `Read ${event.subject || 'Unknown book'}`;
      if (event.kind === 'ingredientEffect') {
        const ingredient = String(event.detail || '').replace(/^Learned after eating:\s*/i, '').trim();
        title.textContent = ingredient
          ? `Ate ${ingredient} · learned ${event.subject || 'Unknown effect'}`
          : `Learned ingredient effect · ${event.subject || 'Unknown effect'}`;
      }
      if (event.kind === 'disenchantment') {
        const learned = String(event.detail || '').replace(/^Learned enchantment:\s*/i, '').trim();
        title.textContent = `Disenchanted ${event.subject || 'Unknown item'} · learned ${learned || 'Unknown enchantment'}`;
      }
      if (event.kind === 'brawl') {
        const opponent = event.subject && event.subject !== 'Unknown opponent' ? ` ${event.stage === 1 ? 'against' : 'to'} ${event.subject}` : '';
        title.textContent = event.stage === 1 ? `Won brawl${opponent}` : event.stage === 2 ? `Lost brawl${opponent}` : `Brawl outcome unavailable${opponent}`;
      }
      if (event.kind === 'follower') title.textContent = `${event.subject || 'Unknown NPC'} ${Number(event.eventType) === 36 ? 'started following you' : 'stopped following you'}`;
      if (event.kind === 'crafting') {
        const eventType = Number(event.eventType);
        if (eventType === 42) {
          const outputFormID = (Number(event.baseFormID || 0) >>> 0).toString(16).toUpperCase().padStart(8, '0');
          title.textContent = `Used materials to craft ${event.subject || 'Unknown item'} (FormID ${outputFormID})`;
        } else {
          const action = eventType === 39 ? 'Tanned' : eventType === 40 ? 'Brewed' : eventType === 41 ? 'Enchanted' : eventType === 43 ? 'Cooked' : 'Smithed';
          title.textContent = `${action} ${event.subject || 'Unknown item'} (FormID ${itemFormID})`;
        }
      }
      if (event.kind === 'quest' && Number(event.eventType) === 19) title.textContent = `Completed quest · ${event.subject || 'Unknown quest'}`;
      if (event.kind === 'quest' && Number(event.eventType) === 20) title.textContent = `Started quest · ${event.subject || 'Unknown quest'}`;
      if (event.kind === 'bounty' && Number(event.eventType) === 26) title.textContent = `${event.crimeBountyApplied ? 'Crime bounty added' : 'Crime bounty failed'} · ${event.subject || 'Recorded crime'}`;
      const generatedIdentityLabels = { spell: 'Spell learned', lockpick: 'Lockpick action', consumption: 'Consumption action', interaction: 'Dialogue action', equipment: 'Equipment change', skill: 'Skill increase', level: 'Player level increase', ingredientEffect: 'Ingredient effect learned', follower: 'Follower status change', crafting: 'Crafting action', disenchantment: 'Disenchantment action', brawl: 'Brawl outcome' };
      const identityLabel = event.identity && !String(event.identity).startsWith('SkyRewrite/')
        ? `Editor ID ${event.identity}`
        : generatedIdentityLabels[event.kind] || (event.kind === 'quest' ? 'Quest stage reached' : event.kind === 'read' ? 'Book read' : 'Location transition');
      const context = [event.kind === 'shopTheft' ? 'Local shop robbery' : event.kind === 'death' ? `Actor FormID ${Number(event.formID || 0).toString(16).toUpperCase().padStart(8, '0')}` : event.kind === 'inventory' ? `Item FormID ${Number(event.formID || 0).toString(16).toUpperCase().padStart(8, '0')}` : identityLabel, event.location || 'Location unavailable'];
      if (['spell', 'shout', 'perk', 'discovery', 'equipment', 'skill', 'read', 'ingredientEffect', 'follower', 'crafting', 'disenchantment', 'brawl'].includes(event.kind)) context.push(`FormID ${Number(event.formID || 0).toString(16).toUpperCase().padStart(8, '0')}`);
      if (event.kind === 'crafting' && Number(event.eventType) === 42 && Number(event.baseFormID)) context.push(`Crafted item FormID ${Number(event.baseFormID).toString(16).toUpperCase().padStart(8, '0')}`);
      if (event.kind === 'perk' && Number(event.baseFormID)) context.push(`Skill tree FormID ${Number(event.baseFormID).toString(16).toUpperCase().padStart(8, '0')}`);
      if (event.kind === 'discovery' && Number(event.baseFormID)) context.push(`Cell FormID ${Number(event.baseFormID).toString(16).toUpperCase().padStart(8, '0')}`);
      if (event.kind === 'ingredientEffect' && Number(event.baseFormID)) context.push(`Ingredient FormID ${Number(event.baseFormID).toString(16).toUpperCase().padStart(8, '0')}`);
      if (event.kind === 'disenchantment' && Number(event.baseFormID)) context.push(`Learned enchantment FormID ${Number(event.baseFormID).toString(16).toUpperCase().padStart(8, '0')}`);
      if (event.kind === 'inventory' && Number(event.baseFormID) && !isTheft) context.push(`Source/destination FormID ${Number(event.baseFormID).toString(16).toUpperCase().padStart(8, '0')}`);
      const formOwner = eventPluginOwner(event);
      const baseOwner = eventPluginOwner(event, 'base');
      const locationOwner = eventPluginOwner(event, 'location');
      if (formOwner) context.push(`FormID owner: ${formOwner}`);
      if (baseOwner && baseOwner !== formOwner) context.push(`Base FormID owner: ${baseOwner}`);
      if (locationOwner && locationOwner !== formOwner) context.push(`Location FormID owner: ${locationOwner}`);
      const showEventDetail = event.detail && (event.kind === 'echo' || event.kind === 'reputation' || event.kind === 'shopTheft' || event.kind === 'bounty' || event.kind === 'lockpick' || event.kind === 'consumption' || event.kind === 'interaction' || event.kind === 'forgery' || event.kind === 'spell' || event.kind === 'shout' || event.kind === 'perk' || event.kind === 'discovery' || event.kind === 'equipment' || event.kind === 'skill' || event.kind === 'level' || event.kind === 'read' || event.kind === 'ingredientEffect' || event.kind === 'follower' || event.kind === 'crafting' || event.kind === 'disenchantment' || event.kind === 'brawl' || (isTheft && !event.isGold) || Number(event.eventType) === 19 || Number(event.eventType) === 20);
      const detail = document.createElement('small'); detail.textContent = showEventDetail ? `${context.join(' · ')} · ${event.detail}` : context.join(' · ');
      detail.hidden = event.kind === 'spell';
      copy.append(title, detail); row.append(day, copy); host.appendChild(row);
    }
    if (state.skyrewriteHistoryHasMore && state.skyrewriteHistoryNextOffset < skyRewriteHistoryMaxRows && items.length < skyRewriteHistoryMaxRows) {
      const loadMore = document.createElement('button');
      loadMore.type = 'button';
      loadMore.className = 'skyrewrite-history-load-more';
      loadMore.disabled = state.skyrewriteHistoryLoading;
      loadMore.textContent = state.skyrewriteHistoryLoading
        ? 'Loading another 100 records…'
        : `Load another ${skyRewriteHistoryPageSize} · ${items.length.toLocaleString()} of ${totalRows.toLocaleString()}`;
      loadMore.addEventListener('click', () => {
        if (state.skyrewriteHistoryLoading) return;
        state.skyrewriteHistoryLoading = true;
        renderSkyRewriteHistoryList(items, settings);
        requestSkyRewriteHistoryPage(state.skyrewriteHistoryNextOffset);
      });
      host.appendChild(loadMore);
    }
  }

  function skyRewriteTabForPanel(panel) {
    if (panel.classList.contains('skyrewrite-intro') || panel.querySelector('#skyrewrite-location-summary, #skyrewrite-recent-activity')) return 'overview';
    if (panel.querySelector('#skyrewrite-regional-trade')) return 'roads';
    if (panel.querySelector('#skyrewrite-reputation-panel')) return 'reputation';
    if (panel.querySelector('#skyrewrite-game-bounty-ledger, #skyrewrite-bounty-ledger')) return 'bounties';
    if (panel.querySelector('#skyrewrite-history')) return 'history';
    if (panel.querySelector('#skyrewrite-theft-history')) return 'thefts';
    if (panel.querySelector('#skyrewrite-place-memory, #skyrewrite-pressure-preview, #skyrewrite-echo-outcomes')) return 'echo';
    if (panel.querySelector('#skyrewrite-actor-memory')) return 'npc';
    return 'plan';
  }

  function setSkyRewriteTab(tab) {
    const page = byId('skyrewrite');
    if (!page) return;
    const availableTabs = new Set(['overview', 'history', 'thefts', 'bounties', 'echo', 'roads', 'reputation', 'npc', 'plan']);
    if (!availableTabs.has(tab)) return;
    const changed = state.skyrewriteTab !== tab;
    state.skyrewriteTab = tab;
    for (const panel of Array.from(page.children).filter((node) => node.tagName === 'SECTION')) {
      panel.dataset.skyrewriteTabPanel = skyRewriteTabForPanel(panel);
      panel.hidden = panel.dataset.skyrewriteTabPanel !== tab;
    }
    document.querySelectorAll('[data-skyrewrite-tab]').forEach((button) => {
      const active = button.dataset.skyrewriteTab === tab;
      button.classList.toggle('active', active);
      button.setAttribute('aria-pressed', active ? 'true' : 'false');
    });
    if (changed && state.skyrewriteVisible) {
      ensureSkyRewriteHistoryLoaded();
      if (tab === 'bounties') invoke('skyos_skyrewrite_game_bounties');
    }
  }

  function scheduleSkyRewriteRender() {
    // PrismaUI can suspend animation-frame callbacks while its game overlay is
    // unfocused. These controls are local filters, so redraw them synchronously.
    SkyRewriteRenderer.setSkyRewriteHistory();
  }

  function latestSkyRewriteEventDay(events, currentDay) {
    let latest = currentDay !== null && currentDay !== undefined && Number.isFinite(Number(currentDay)) ? Number(currentDay) : -Infinity;
    for (const event of events) {
      if (event.day === null || event.day === undefined || event.day === '') continue;
      const day = Number(event.day);
      if (Number.isFinite(day)) latest = Math.max(latest, day);
    }
    return Number.isFinite(latest) ? latest : null;
  }

  function isSkyRewritePlayerKill(event) {
    return (event?.kind === 'death' || Number(event?.eventType) === 1) && String(event?.detail || '').trim() === 'Player killed this actor';
  }

  function isSkyRewriteTheft(event) {
    return event?.isTheft === true || Number(event?.eventType) === 10 || String(event?.identity || '').startsWith('SkyRewrite/Theft/v1');
  }

  function renderSkyRewriteTheftHistory(events) {
    const host = byId('skyrewrite-theft-history');
    const count = byId('skyrewrite-theft-count');
    if (!host || !count) return;
    const thefts = events.filter(isSkyRewriteTheft);
    const query = String(state.skyrewriteTheftSearch || '').trim().toLocaleLowerCase();
    const filtered = thefts.filter((event) => {
      const isGold = event.isGold === true || (Number(event.formID) >>> 0) === 0x0000000F;
      const pickpocket = event.theftSourceKind === 'pickpocket' || String(event.identity || '').startsWith('SkyRewrite/Theft/v1/pickpocket/');
      if (state.skyrewriteTheftFilter === 'pickpocket' && !pickpocket) return false;
      if (state.skyrewriteTheftFilter === 'gold' && !isGold) return false;
      if (state.skyrewriteTheftFilter === 'items' && isGold) return false;
      const haystack = [event.subject, event.sourceName, event.location, event.detail, event.formPlugin, event.basePlugin, event.locationPlugin, event.formID, event.day, event.holdKey].join(' ').toLocaleLowerCase();
      return !query || haystack.includes(query);
    });
    const savedThefts = Number(state.skyrewriteReputation?.thefts);
    const lifetimeThefts = Number.isFinite(savedThefts) ? Math.max(0, savedThefts) : thefts.length;
    count.textContent = `${filtered.length} shown · ${thefts.length} retained · ${lifetimeThefts} total`;
    document.querySelectorAll('[data-skyrewrite-theft-filter]').forEach((button) => {
      const active = button.dataset.skyrewriteTheftFilter === state.skyrewriteTheftFilter;
      button.classList.toggle('active', active);
      button.setAttribute('aria-pressed', active ? 'true' : 'false');
    });
    host.replaceChildren();
    if (!filtered.length) {
      const empty = document.createElement('div'); empty.className = 'empty';
      empty.textContent = thefts.length ? 'No stolen goods match this filter.' : 'No theft events have been recorded in this save yet.';
      host.appendChild(empty);
      return;
    }
    for (const event of filtered) {
      const row = document.createElement('article'); row.className = 'skyrewrite-theft-row';
      const heading = document.createElement('div'); heading.className = 'skyrewrite-theft-heading';
      const isGold = event.isGold === true || (Number(event.formID) >>> 0) === 0x0000000F;
      const quantity = Math.max(1, Number(event.stage) || Number(String(event.detail || '').match(/\bstole\s+(\d+)/i)?.[1]) || 1);
      const item = event.subject || (isGold ? 'Septims' : 'Unknown item');
      const sourceName = event.sourceName || (String(event.identity || '').startsWith('SkyRewrite/Theft/v1/pickpocket/') ? String(event.identity).slice('SkyRewrite/Theft/v1/pickpocket/'.length) : '');
      const formID = (Number(event.formID) >>> 0).toString(16).toUpperCase().padStart(8, '0');
      const title = document.createElement('strong'); title.textContent = sourceName
        ? `Pickpocketed ${sourceName}`
        : `Stole ${isGold ? `${quantity} septims` : item} (FormID ${formID})${!isGold && quantity > 1 ? ` ×${quantity}` : ''}`;
      const day = document.createElement('span'); day.textContent = hasSkyRewriteEventDay(event) ? `Day ${Number(event.day).toFixed(1)}` : 'Day unknown';
      heading.append(title, day);
      const detail = document.createElement('small');
      const source = sourceName ? `Item: ${item} ×${quantity}` : 'Taken from an owned container or object';
      const place = event.location || 'Location unavailable';
      detail.textContent = `${source} · ${place}${event.holdKey ? ` · ${String(event.holdKey).replace(/^./, (letter) => letter.toUpperCase())} hold` : ''}`;
      const metadata = document.createElement('small');
      metadata.textContent = `FormID ${(Number(event.formID) >>> 0).toString(16).toUpperCase().padStart(8, '0')}${event.formPlugin ? ` · ${event.formPlugin}` : ''}`;
      row.append(heading, detail, metadata); host.appendChild(row);
    }
  }

  function renderSkyRewriteRegionalTrade(events) {
    const host = byId('skyrewrite-regional-trade');
    const count = byId('skyrewrite-regional-trade-count');
    if (!host || !count) return;
    const holds = [
      ['eastmarch', 'Eastmarch', 'Windhelm'], ['falkreath', 'Falkreath', 'Falkreath'],
      ['haafingar', 'Haafingar', 'Solitude'], ['hjaalmarch', 'Hjaalmarch', 'Morthal'],
      ['pale', 'The Pale', 'Dawnstar'], ['reach', 'The Reach', 'Markarth'],
      ['rift', 'The Rift', 'Riften'], ['whiterun', 'Whiterun', 'Whiterun'],
      ['winterhold', 'Winterhold', 'Winterhold'], ['solstheim', 'Solstheim', 'Raven Rock']
    ];
    const categories = [
      ['bandit', 'Bandits'], ['dragon', 'Dragons'], ['forsworn', 'Forsworn'],
      ['vampire', 'Vampires'], ['wolf', 'Wolves'], ['bear', 'Bears'],
      ['spider', 'Spiders'], ['draugr', 'Draugrs'], ['falmer', 'Falmers'], ['dragonpriest', 'Dragon Priests'],
      ['skeleton', 'Skeletons'], ['hagraven', 'Hagravens'], ['dwarven', 'Dwarven creatures'],
      ['monster', 'Other hostile creatures'], ['otherhostile', 'Other hostile actors'], ['allhostile', 'All hostile defeats'],
      ['civilians', 'Civilians killed'], ['thefts', 'Theft events'], ['itemsstolen', 'Items stolen'], ['goldstolen', 'Gold stolen'], ['favorquests', 'Favor quests']
    ];
    const totals = new Map(holds.map(([key]) => [key, Object.fromEntries(categories.map(([category]) => [category, 0]))]));
    const savedCategories = new Set();
    const setSaved = (holdKey, category, value) => {
      if (!totals.has(holdKey) || !(category in totals.get(holdKey))) return;
      totals.get(holdKey)[category] = Math.max(totals.get(holdKey)[category], Math.max(0, Number(value) || 0));
      savedCategories.add(`${holdKey}:${category}`);
    };
    for (const saved of state.skyrewriteRegionalBanditStates) setSaved(saved.holdKey, 'bandit', saved.kills);
    for (const saved of state.skyrewriteRegionalBanditHoldStates) setSaved(saved.holdKey, 'bandit', saved.kills);
    for (const saved of state.skyrewriteRegionalCreatureStates) {
      const holdKey = String(saved.holdKey || '');
      if (holdKey !== 'all') setSaved(holdKey, String(saved.category || ''), saved.count);
    }
    for (const saved of state.skyrewriteRegionalFavorStates) setSaved(String(saved.holdKey || ''), 'favorquests', saved.count);

    const inferred = (event, category) => {
      const field = { bandit: 'victimBandit', dragon: 'victimDragon', forsworn: 'victimForsworn', vampire: 'victimVampire', wolf: 'victimWolf', bear: 'victimBear', spider: 'victimSpider', monster: 'victimMonster', allhostile: 'victimHostile', civilians: 'victimCivilian' }[category];
      if (event[field] === true) return true;
      const text = `${event.subject || ''} ${event.identity || ''}`.toLocaleLowerCase();
      if (event[field] === false) return false;
      if (category === 'bandit') return /bandit|marauder/.test(text);
      if (category === 'dragon') return /dragon/.test(text) && !/dragon ?priest/.test(text);
      if (category === 'forsworn') return /forsworn/.test(text);
      if (category === 'vampire') return /vampire/.test(text);
      if (category === 'wolf') return /wolf/.test(text);
      if (category === 'bear') return /bear/.test(text);
      if (category === 'spider') return /spider/.test(text);
      if (category === 'draugr') return /draugr/.test(text);
      if (category === 'falmer') return /falmer/.test(text);
      if (category === 'dragonpriest') return /dragon ?priest/.test(text);
      if (category === 'skeleton') return /skeleton/.test(text);
      if (category === 'hagraven') return /hagraven/.test(text);
      if (category === 'dwarven') return /dwarven|dwemer|centurion|sphere guardian|spider guardian|ballista/.test(text);
      if (category === 'civilians') return event.victimCivilian === true;
      if (category === 'monster') return /dragon|wolf|bear|spider|sabre ?cat|troll|giant|draugr|falmer|dragon ?priest|skeleton|skeever|spriggan|hagraven|dwarven|dwemer|centurion|atronach|gargoyle|ash spawn|seeker|lurker|reaver|riekling|werewolf|werebear|chaurus|mudcrab|slaughterfish/.test(text);
      if (category === 'otherhostile') return event.victimOtherHostile === true;
      if (category === 'allhostile') return event.victimHostile === true || /bandit|marauder|dragon|forsworn|vampire|wolf|bear|spider|sabre ?cat|troll|giant|draugr|falmer|dragon ?priest|skeleton|skeever|spriggan|hagraven|dwarven|dwemer|centurion|atronach|gargoyle|ash spawn|seeker|lurker|reaver|riekling|werewolf|werebear|chaurus/.test(text);
      return false;
    };
    for (const event of events) {
      const holdKey = String(event.holdKey || '').toLocaleLowerCase();
      if (!totals.has(holdKey)) continue;
      if (isSkyRewritePlayerKill(event)) {
        for (const [category] of categories) {
          if (['thefts', 'itemsstolen', 'goldstolen'].includes(category)) continue;
          const savedKey = `${holdKey}:${category}`;
          if (savedCategories.has(savedKey) || !inferred(event, category)) continue;
          totals.get(holdKey)[category]++;
        }
      } else if (isSkyRewriteTheft(event)) {
        const row = totals.get(holdKey);
        const quantity = Math.max(1, Number(event.stage) || Number(String(event.detail || '').match(/\bstole\s+(\d+)/i)?.[1]) || 1);
        if (!savedCategories.has(`${holdKey}:thefts`)) row.thefts++;
        if (!savedCategories.has(`${holdKey}:itemsstolen`)) row.itemsstolen += quantity;
        if ((event.isGold === true || (Number(event.formID) >>> 0) === 0x0000000F) && !savedCategories.has(`${holdKey}:goldstolen`)) row.goldstolen += quantity;
      }
    }
    const retainedBanditKills = events.filter((event) => isSkyRewritePlayerKill(event) && inferred(event, 'bandit')).length;
    const totalBanditKills = state.skyrewriteRegionalBanditKills === null
      ? retainedBanditKills
      : Math.max(Math.max(0, Number(state.skyrewriteRegionalBanditKills) || 0), retainedBanditKills);
    const totalAssigned = [...totals.values()].reduce((sum, row) => sum + row.allhostile, 0);
    count.textContent = `${totalBanditKills} saved bandit defeats · ${totalAssigned} hostile defeats assigned to holds · 10 regions`;
    document.querySelectorAll('[data-skyrewrite-hold]').forEach((button) => {
      const active = button.dataset.skyrewriteHold === state.skyrewriteHold;
      button.classList.toggle('active', active);
      button.setAttribute('aria-pressed', String(active));
    });
    host.replaceChildren();

    const hold = holds.find(([key]) => key === state.skyrewriteHold) || holds[0];
    const holdStats = totals.get(hold[0]);
    const localReputation = holdStats.bandit + holdStats.forsworn + holdStats.vampire + holdStats.monster + holdStats.favorquests - holdStats.thefts - holdStats.civilians * 5;
    const priceModifier = localReputation < 0 ? Math.min(500, Math.abs(localReputation)) : -Math.min(99, localReputation);
    const global = document.createElement('article'); global.className = 'skyrewrite-memory-row skyrewrite-regional-price';
    const globalHeading = document.createElement('div'); globalHeading.className = 'skyrewrite-memory-heading';
    const globalName = document.createElement('strong'); globalName.textContent = 'Local purchase-price response';
    const globalState = document.createElement('span'); globalState.className = `skyrewrite-memory-state ${priceModifier > 0 ? 'price-markup' : priceModifier < 0 ? 'price-discount' : ''}`;
    globalState.textContent = priceModifier > 0 ? `+${priceModifier}% · 500% cap` : priceModifier < 0 ? `${priceModifier}% · 99% cap` : 'Standard prices';
    globalHeading.append(globalName, globalState);
    const globalNote = document.createElement('small');
    globalNote.textContent = `${hold[1]} reputation ${localReputation >= 0 ? '+' : ''}${localReputation}. Each negative point adds 1% to the base purchase price; each positive point reduces it by 1%. Skyrim’s other barter modifiers still apply.`;
    global.append(globalHeading, globalNote); host.appendChild(global);

    const heading = document.createElement('div'); heading.className = 'skyrewrite-hold-heading';
    const title = document.createElement('h3'); title.textContent = hold[1];
    const capital = document.createElement('span'); capital.textContent = `Capital · ${hold[2]}`;
    heading.append(title, capital); host.appendChild(heading);
    const grid = document.createElement('div'); grid.className = 'skyrewrite-counter-grid';
    for (const [category, label] of categories) {
      const card = document.createElement('article'); card.className = 'skyrewrite-counter-card';
      const value = document.createElement('strong'); value.textContent = String(holdStats[category]);
      const caption = document.createElement('span'); caption.textContent = label;
      card.append(value, caption); grid.appendChild(card);
    }
    host.appendChild(grid);
    const bountyActions = document.createElement('div'); bountyActions.className = 'skyrewrite-hold-bounty-action';
    const bountyCopy = document.createElement('div');
    const bountyTitle = document.createElement('strong'); bountyTitle.textContent = `Clear ${hold[1]} crime balance`;
    const bountyNote = document.createElement('small'); bountyNote.textContent = 'Removes Skyrim’s hold bounty (violent and nonviolent) and resolves any pending SkyRewrite temporary bounty for this hold.';
    bountyCopy.append(bountyTitle, bountyNote);
    const clearBounty = document.createElement('button'); clearBounty.type = 'button'; clearBounty.className = 'skyrewrite-clear-bounty'; clearBounty.textContent = 'Clear both bounties'; clearBounty.dataset.clearHoldBounty = hold[0];
    bountyActions.append(bountyCopy, clearBounty); host.appendChild(bountyActions);
    if (totalBanditKills === 0 && totalAssigned === 0 && holdStats.thefts === 0 && holdStats.civilians === 0) {
      const note = document.createElement('div'); note.className = 'skyrewrite-region-note';
      note.textContent = 'No regional hostile defeats have been saved yet. New kills populate the matching hold or Solstheim when the current location can be resolved.';
      host.appendChild(note);
    }
  }

  function renderSkyRewriteReputation() {
    const host = byId('skyrewrite-reputation-panel');
    const rank = byId('skyrewrite-reputation-rank');
    if (!host || !rank) return;
    const saved = state.skyrewriteReputation || {};
    const score = Number.isFinite(Number(saved.score)) ? Math.trunc(Number(saved.score)) : 0;
    const negativeTitles = [
      { min: -50, max: -1, name: 'Unwelcome', description: 'Nearby residents recognize you as a source of trouble and greet you with anger.', style: 'unwelcome' },
      { min: -150, max: -51, name: 'Shunned', description: 'Some residents refuse to speak with you; vendors and locals become openly distrustful.', style: 'shunned' },
      { min: -200, max: -151, name: 'Hunted', description: 'Some residents flee when you approach, while local guards remain alert to your crimes.', style: 'hunted' },
      { min: -350, max: -201, name: 'Outlaw', description: 'A portion of nearby non-hostile residents will attack rather than welcome you.', style: 'outlaw' },
      { min: -500, max: -351, name: 'Public Enemy', description: 'Most nearby residents flee; some stand their ground and attack.', style: 'public-enemy' }
    ];
    const titles = [
      { min: 0, max: 50, name: 'Nameless Wanderer', description: 'A stranger with no deeds worth remembering.', style: 'nameless-wanderer' },
      { min: 51, max: 100, name: 'Rumored Blade', description: 'Tavern stories are beginning to mention your name.', style: 'rumored-blade' },
      { min: 101, max: 150, name: 'Hold-Known', description: 'Your deeds have reached the ears of local guards and Jarls.', style: 'hold-known' },
      { min: 151, max: 200, name: 'Songforged', description: "Your exploits have earned a place in the bards' tales.", style: 'songforged' },
      { min: 201, max: 250, name: 'Shield of the Holds', description: 'A proven hero whose deeds protect the people of Skyrim.', style: 'shield-of-the-holds' },
      { min: 251, max: 300, name: 'Dragonsworn Hero', description: 'A name spoken with reverence in the shadow of dragons.', style: 'dragonsworn-hero' },
      { min: 301, max: 350, name: 'Oathkeeper of Ysgramor', description: 'A mighty adventurer worthy of the old Nordic traditions.', style: 'oathkeeper' },
      { min: 351, max: 400, name: 'Voice of the North', description: 'Your reputation echoes from the tundra to the Throat of the World.', style: 'voice-of-the-north' },
      { min: 401, max: 450, name: 'Saga-Born', description: 'Your life has become the kind of saga future generations remember.', style: 'saga-born' },
      { min: 451, max: 500, name: 'Pride of Skyrim', description: 'A name honored throughout every Hold.', style: 'pride-of-skyrim' },
      { min: 501, max: 999, name: 'Saga of the Living', description: 'Your deeds are already becoming the legends of tomorrow.', style: 'saga-of-the-living' },
      { min: 1000, max: Infinity, name: 'Eternal Song of Sovngarde', description: 'A mythic figure whose deeds will be sung for eternity.', style: 'eternal-song' }
    ];
    const allTitles = [...negativeTitles, ...titles];
    const currentTitle = score < 0
      ? negativeTitles.find((title) => score >= title.min && score <= title.max) || negativeTitles[negativeTitles.length - 1]
      : titles.find((title) => score >= title.min && score <= title.max) || titles[0];
    const relationshipRank = score >= 201 ? 4 : score >= 100 ? 3 : score >= 50 ? 2 : score >= 35 ? 1 : 0;
    const giftCount = Math.max(0, Number(saved.giftMilestone) || 0);
    const lastGiftScore = Math.max(0, Number(saved.giftScore) || 0);
    const murders = Math.max(0, Number(saved.murders) || 0);
    const npcResponse = score > 0 ? 'Suppressed while reputation is positive'
      : score <= -351 ? 'Most flee; some attack'
        : score <= -201 ? 'Some nearby residents attack'
          : score <= -151 ? 'Some flee; guards stay alert'
            : score <= -51 ? 'Some residents refuse dialogue'
              : score < 0 ? 'Nearby residents show anger'
                : murders > 500 ? 'All nearby NPCs flee'
                  : murders > 200 ? 'Some nearby NPCs attack'
                    : murders > 100 ? 'Some nearby NPCs refuse dialogue'
                      : murders > 50 ? 'Nearby NPCs show anger'
                        : `${51 - murders} murders to first response`;
    const nextGift = (() => {
      if (score < 0) return 5;
      if (score < 35) return (Math.floor(score / 5) + 1) * 5;
      if (score < 50) return Math.min(50, 35 + (Math.floor((score - 35) / 4) + 1) * 4);
      if (score < 100) return Math.min(100, 50 + (Math.floor((score - 50) / 3) + 1) * 3);
      if (score < 201) return Math.min(201, 100 + (Math.floor((score - 100) / 2) + 1) * 2);
      return score + 1;
    })();
    const remaining = Math.max(0, nextGift - score);
    rank.textContent = `${currentTitle.name} · ${score} reputation`;
    host.replaceChildren();
    const hero = document.createElement('section'); hero.className = `skyrewrite-reputation-summary skyrewrite-current-title skyrewrite-title-${currentTitle.style}`;
    const seal = document.createElement('span'); seal.className = 'skyrewrite-title-seal'; seal.textContent = score < 0 ? '!' : score >= 1000 ? '✦' : String(titles.indexOf(currentTitle) + 1).padStart(2, '0');
    const copy = document.createElement('div'); copy.className = 'skyrewrite-current-title-copy';
    const eyebrow = document.createElement('small'); eyebrow.textContent = `REPUTATION TITLE · ${currentTitle.min}–${Number.isFinite(currentTitle.max) ? currentTitle.max : '∞'} REP`;
    const name = document.createElement('strong'); name.textContent = currentTitle.name;
    const description = document.createElement('span'); description.textContent = currentTitle.description;
    copy.append(eyebrow, name, description);
    const total = document.createElement('b'); total.className = 'skyrewrite-reputation-score'; total.textContent = String(score);
    const scoreLabel = document.createElement('small'); scoreLabel.className = 'skyrewrite-reputation-score-label'; scoreLabel.textContent = 'REP';
    const scoreBlock = document.createElement('div'); scoreBlock.className = 'skyrewrite-reputation-score-block'; scoreBlock.append(total, scoreLabel);
    hero.append(seal, copy, scoreBlock); host.appendChild(hero);

    const effects = document.createElement('div'); effects.className = 'skyrewrite-reputation-effects';
    const effectCards = [
      ['Next gift', score < 0 ? `Return to +5 REP · ${remaining} points to go` : `${nextGift} REP · ${remaining} to go`],
      ['Nearby friendly rank', score < 0 ? 'Unavailable while reputation is negative' : relationshipRank ? `${relationshipRank} / 4` : `Unlocks at 35 REP`],
      ['Gifts earned', giftCount ? `${giftCount} · last at ${lastGiftScore} REP` : 'None yet'],
      ['Nearby NPC behavior', npcResponse]
    ];
    for (const [label, value] of effectCards) {
      const card = document.createElement('article'); card.className = 'skyrewrite-reputation-effect';
      const heading = document.createElement('small'); heading.textContent = label;
      const detail = document.createElement('strong'); detail.textContent = value;
      card.append(heading, detail); effects.appendChild(card);
    }
    host.appendChild(effects);

    const grid = document.createElement('div'); grid.className = 'skyrewrite-counter-grid skyrewrite-reputation-grid';
    const components = [
      ['Eligible quests completed', saved.favorQuests], ['Bandits defeated', saved.bandits],
      ['Forsworn defeated', saved.forsworn], ['Vampires defeated', saved.vampires],
      ['Dragons defeated', saved.dragons], ['Draugrs defeated', saved.draugrs], ['Falmers defeated', saved.falmers],
      ['Dragon Priests defeated', saved.dragonPriests], ['Skeletons defeated', saved.skeletons],
      ['Hagravens defeated', saved.hagravens], ['Dwarven creatures defeated', saved.dwarvenCreatures],
      ['Wolves defeated', saved.wolves], ['Bears defeated', saved.bears], ['Spiders defeated', saved.spiders],
      ['Other hostile monsters defeated', saved.monsters], ['Other hostile actors defeated', saved.otherHostile],
      ['All hostile defeats', saved.allHostile], ['Murders', saved.murders],
      ['Theft events', saved.thefts], ['Forgery jobs', saved.forgeries], ['Items stolen', saved.itemsStolen], ['Gold stolen', saved.goldStolen]
    ];
    for (const [label, value] of components) {
      const card = document.createElement('article'); card.className = 'skyrewrite-counter-card';
      const number = document.createElement('strong'); number.textContent = String(Math.max(0, Number(value) || 0));
      const caption = document.createElement('span'); caption.textContent = label;
      card.append(number, caption); grid.appendChild(card);
    }
    host.appendChild(grid);

    const titleHeading = document.createElement('div'); titleHeading.className = 'skyrewrite-title-ladder-heading';
    const ladderTitle = document.createElement('strong'); ladderTitle.textContent = 'Reputation titles';
    const ladderNote = document.createElement('span'); ladderNote.textContent = 'Renown and infamy both shape nearby NPC reactions.';
    titleHeading.append(ladderTitle, ladderNote); host.appendChild(titleHeading);
    const ladder = document.createElement('div'); ladder.className = 'skyrewrite-title-ladder';
    for (const [index, tier] of allTitles.entries()) {
      const card = document.createElement('article');
      const earned = tier.max < 0 ? score <= tier.max : score >= tier.min;
      const isCurrent = tier === currentTitle;
      card.className = `skyrewrite-title-tier skyrewrite-title-${tier.style}${isCurrent ? ' is-current' : earned ? ' is-earned' : ' is-locked'}`;
      const tierHeader = document.createElement('div'); tierHeader.className = 'skyrewrite-title-tier-header';
      const numeral = document.createElement('span'); numeral.textContent = tier.max < 0 ? `N${index + 1}` : index - negativeTitles.length === 11 ? '✦' : String(index - negativeTitles.length + 1).padStart(2, '0');
      const range = document.createElement('small'); range.textContent = `${tier.min}–${Number.isFinite(tier.max) ? tier.max : '∞'} REP`;
      tierHeader.append(numeral, range);
      const tierName = document.createElement('strong'); tierName.textContent = tier.name;
      const tierDescription = document.createElement('p'); tierDescription.textContent = tier.description;
      const stateLabel = document.createElement('span'); stateLabel.className = 'skyrewrite-title-tier-state'; stateLabel.textContent = isCurrent ? 'CURRENT TITLE' : earned ? 'ATTAINED' : 'LOCKED';
      card.append(tierHeader, tierName, tierDescription, stateLabel); ladder.appendChild(card);
    }
    host.appendChild(ladder);
    const note = document.createElement('p'); note.className = 'skyrewrite-region-note skyrewrite-reputation-note';
    note.textContent = `Negative reputation is capped at −500 and divided into five titles. Residents show anger from −1, some refuse dialogue from −51, some flee from −151, some attack from −201, and at −351 most flee while some attack. Positive-standing rewards and relationship improvements remain available as reputation recovers. Gifts arrive every 5 reputation before 35, then every 4 points to 50, every 3 to 100, every 2 to 201, and each point after that.`;
    host.appendChild(note);
  }

  function isSkyRewriteEchoEvent(event) {
    return Number(event?.eventType) === 5 || event?.kind === 'echo' && /Local Echo rule fired/i.test(String(event?.detail || ''));
  }

  function hasSkyRewriteEventDay(event) {
    return event?.day !== null && event?.day !== undefined && event?.day !== '' && Number.isFinite(Number(event.day));
  }

  function renderSkyRewriteEchoOutcomes(events) {
    const host = byId('skyrewrite-echo-outcomes');
    const count = byId('skyrewrite-echo-outcome-count');
    if (!host || !count) return;
    const labels = new Map([[5, 'Local Echo fired'], [6, 'Temporary hold bounty applied'], [7, 'Temporary hold bounty resolved'], [8, 'Adaptive guard response applied'], [9, 'Adaptive guard response expired'], [11, 'Theft bounty trigger skipped'], [12, 'Local Echo cooldown active'], [13, 'Hold guard dispatch']]);
    const outcomes = events.filter((event) => labels.has(Number(event.eventType)) || event?.kind === 'echo' && /Local Echo rule fired/i.test(String(event?.detail || '')));
    count.textContent = `${outcomes.length} recorded`;
    host.replaceChildren();
    if (!outcomes.length) {
      const empty = document.createElement('div'); empty.className = 'empty'; empty.textContent = 'No Echo responses have been recorded in this save yet.'; host.appendChild(empty);
      return;
    }
    for (const event of outcomes) {
      const row = document.createElement('article'); row.className = 'skyrewrite-echo-outcome';
      const heading = document.createElement('div'); heading.className = 'skyrewrite-echo-outcome-heading';
      const eventType = Number(event.eventType) || (/Local Echo rule fired/i.test(String(event.detail || '')) ? 5 : 0);
      const outcomeLabel = eventType === 13 ? (String(event.detail || '').includes('spawned') ? 'Hold guard dispatched' : 'Hold guard dispatch failed') : labels.get(eventType) || 'Recorded Echo response';
      const title = document.createElement('strong'); title.textContent = `${outcomeLabel} · ${event.subject || event.location || 'Unknown location'}`;
      const day = document.createElement('span'); day.textContent = hasSkyRewriteEventDay(event) ? `Day ${Number(event.day).toFixed(1)}` : 'Day unknown';
      heading.append(title, day);
      const detail = document.createElement('small'); detail.textContent = event.detail || 'No response details were retained.';
      row.append(heading, detail); host.appendChild(row);
    }
  }

  function renderSkyRewriteBountyLedger(events, currentDay) {
    const host = byId('skyrewrite-bounty-ledger');
    const count = byId('skyrewrite-bounty-count');
    if (!host || !count) return;
    const applied = events.filter((event) => Number(event.eventType) === 6);
    const resolutions = new Map(events.filter((event) => Number(event.eventType) === 7).map((event) => [String(event.identity || ''), event]));
    host.replaceChildren();
    if (!applied.length) {
      count.textContent = '0 shown · 0 active · 0 due · 0 recorded';
      const empty = document.createElement('div'); empty.className = 'empty'; empty.textContent = 'No temporary hold bounties have been recorded in this save.'; host.appendChild(empty);
      return;
    }
    const records = applied.map((event) => {
      const identity = String(event.identity || '');
      const resolution = resolutions.get(identity);
      const appliedDay = hasSkyRewriteEventDay(event) ? Number(event.day) : NaN;
      const identityParts = identity.split('/');
      const parsedDuration = identityParts.length >= 6 ? Number(identityParts[identityParts.length - 1]) : 1;
      const duration = Number.isFinite(parsedDuration) && parsedDuration >= 0.5 && parsedDuration <= 7 ? parsedDuration : 1;
      const expiryDay = Number.isFinite(appliedDay) ? appliedDay + duration : null;
      const detail = String(resolution?.detail || '');
      const status = resolution ? (detail.includes('manually removed') ? 'undone' : detail.includes('verification failed') ? 'failed' : detail.includes('removed') ? 'expired' : 'skipped') : (expiryDay !== null && Number.isFinite(currentDay) && currentDay >= expiryDay ? 'due' : 'active');
      const trigger = events.filter((candidate) => isSkyRewriteEchoEvent(candidate) && (candidate.subject === event.subject || candidate.location === event.location))
        .sort((a, b) => Math.abs(Number(a.day || 0) - appliedDay) - Math.abs(Number(b.day || 0) - appliedDay))[0] || null;
      const parsedAmount = Number(String(event.detail || '').match(/Bounty\s+\+(\d+)/i)?.[1]);
      const stageAmount = Number(event.stage);
      return { event, identity, resolution, appliedDay, expiryDay, duration, status, detail, trigger, amount: Number.isFinite(stageAmount) && stageAmount > 0 ? stageAmount : Number.isFinite(parsedAmount) ? parsedAmount : 0 };
    });
    const activeCount = records.filter((record) => record.status === 'active').length;
    const dueCount = records.filter((record) => record.status === 'due').length;
    const ordered = records.sort((a, b) => (b.appliedDay || 0) - (a.appliedDay || 0));
    count.textContent = `${ordered.length} recorded · ${activeCount} active · ${dueCount} due`;
    if (!ordered.length) {
      const empty = document.createElement('div'); empty.className = 'empty';
      empty.textContent = 'No temporary hold bounties have been recorded in this save.';
      host.appendChild(empty); return;
    }
    for (const record of ordered) {
      const { event, identity, resolution, appliedDay, expiryDay, status, detail: resolutionDetail, trigger, amount } = record;
      const row = document.createElement('article'); row.className = 'skyrewrite-bounty-row';
      const heading = document.createElement('div'); heading.className = 'skyrewrite-bounty-heading';
      const title = document.createElement('strong'); title.textContent = event.subject || 'Unknown hold';
      const badge = document.createElement('span'); badge.className = `skyrewrite-bounty-state${status === 'active' || status === 'due' ? '' : ' resolved'}`; badge.textContent = status === 'expired' ? 'Expired · removed' : status === 'undone' ? 'Cleared manually' : status === 'failed' ? 'Clear verification failed' : status === 'skipped' ? (resolutionDetail.toLowerCase().includes('manual undo') ? 'Clear skipped · balance changed' : 'Expiry skipped · balance preserved') : status === 'due' ? 'Expiry due · awaiting transition' : 'Active';
      heading.append(title, badge);
      row.appendChild(heading);
      host.appendChild(row);
      const detail = document.createElement('small');
      const timing = expiryDay === null ? 'Expiry day unavailable' : `Applied day ${appliedDay.toFixed(1)} · expires after day ${expiryDay.toFixed(1)}`;
      const remaining = !resolution && expiryDay !== null && Number.isFinite(currentDay) && currentDay < expiryDay ? ` · ${Math.max(0, expiryDay - currentDay).toFixed(1)} days remaining` : '';
      detail.textContent = `Actual applied amount: ${amount} septims · ${timing}${remaining}`;
      row.appendChild(detail);
      const application = document.createElement('small'); application.className = 'skyrewrite-bounty-application'; application.textContent = String(event.detail || 'Application settings were not retained with this record.');
      row.appendChild(application);
      if (resolution) {
        const outcome = document.createElement('small'); outcome.className = 'skyrewrite-bounty-application'; outcome.textContent = resolutionDetail;
        row.appendChild(outcome);
      }
      const footer = document.createElement('div'); footer.className = 'skyrewrite-bounty-footer';
      const provenance = document.createElement('small'); provenance.textContent = trigger?.detail || 'Local Echo trigger detail is unavailable in the retained journal.';
      const historyButton = document.createElement('button'); historyButton.type = 'button'; historyButton.className = 'skyrewrite-memory-history'; historyButton.textContent = 'Open rule history'; historyButton.dataset.openBountyHistory = 'true'; historyButton.dataset.location = event.location || '';
      footer.append(provenance);
      const actions = document.createElement('div'); actions.className = 'skyrewrite-bounty-actions';
      actions.appendChild(historyButton);
      if (!resolution) {
        const undoButton = document.createElement('button'); undoButton.type = 'button'; undoButton.className = 'skyrewrite-memory-history'; undoButton.textContent = 'Clear bounty'; undoButton.dataset.undoBounty = 'true'; undoButton.dataset.identity = identity;
        actions.appendChild(undoButton);
      }
      footer.appendChild(actions); row.appendChild(footer);
    }
  }

  function renderSkyRewriteGameBountyLedger(bounties, events) {
    const host = byId('skyrewrite-game-bounty-ledger');
    const count = byId('skyrewrite-game-bounty-count');
    const historyHost = byId('skyrewrite-game-bounty-history');
    const historyCount = byId('skyrewrite-game-bounty-history-count');
    if (!host || !count || !historyHost || !historyCount) return;
    const active = Array.isArray(bounties) ? bounties.slice() : [];
    active.sort((a, b) => String(a.hold || '').localeCompare(String(b.hold || '')));
    count.textContent = `${active.length} active hold${active.length === 1 ? '' : 's'}`;
    host.replaceChildren();
    if (!active.length) {
      const empty = document.createElement('div'); empty.className = 'empty'; empty.textContent = 'No active Skyrim crime bounty was found in the loaded holds.'; host.appendChild(empty);
    } else {
      for (const bounty of active) {
        const row = document.createElement('article'); row.className = 'skyrewrite-bounty-row skyrewrite-game-bounty-row';
        const heading = document.createElement('div'); heading.className = 'skyrewrite-bounty-heading';
        const title = document.createElement('strong'); title.textContent = bounty.hold || 'Unknown hold';
        const badge = document.createElement('span'); badge.className = 'skyrewrite-bounty-state'; badge.textContent = 'Active';
        heading.append(title, badge); row.appendChild(heading);
        const totals = document.createElement('small'); totals.className = 'skyrewrite-bounty-application'; totals.textContent = `Total ${Number(bounty.total) || 0} septims · Violent ${Number(bounty.violent) || 0} · Nonviolent ${Number(bounty.nonViolent) || 0}${bounty.formPlugin ? ` · Faction ${bounty.formPlugin}` : ''}`; row.appendChild(totals);
        const actions = document.createElement('div'); actions.className = 'skyrewrite-bounty-actions';
        const clear = document.createElement('button'); clear.type = 'button'; clear.className = 'skyrewrite-clear-button'; clear.textContent = 'Clear bounty'; clear.dataset.clearGameBounty = bounty.holdKey || ''; actions.appendChild(clear);
        row.appendChild(actions); host.appendChild(row);
      }
    }
    const cleared = events.filter((event) => event.kind === 'bounty' && Number(event.eventType) === 18);
    historyCount.textContent = `${cleared.length} recorded`;
    historyHost.replaceChildren();
    if (!cleared.length) {
      const empty = document.createElement('div'); empty.className = 'empty'; empty.textContent = 'No Skyrim bounty clear actions have been recorded yet.'; historyHost.appendChild(empty); return;
    }
    for (const event of cleared) {
      const row = document.createElement('article'); row.className = 'skyrewrite-bounty-row skyrewrite-bounty-history-row';
      const heading = document.createElement('div'); heading.className = 'skyrewrite-bounty-heading';
      const title = document.createElement('strong'); title.textContent = event.subject || event.location || 'Skyrim hold';
      const badge = document.createElement('span'); badge.className = 'skyrewrite-bounty-state resolved'; badge.textContent = 'Cleared';
      const day = document.createElement('small'); day.textContent = event.day != null && Number.isFinite(Number(event.day)) ? `Day ${Number(event.day).toFixed(1)}` : 'Day unknown';
      heading.append(title, badge); row.append(heading, day);
      const detail = document.createElement('small'); detail.className = 'skyrewrite-bounty-application'; detail.textContent = event.detail || 'Skyrim bounty balance cleared.'; row.appendChild(detail);
      historyHost.appendChild(row);
    }
  }


  function renderSkyRewritePicker(name, options, selectedValue) {
    const picker = document.querySelector(`[data-skyrewrite-picker="${name}"]`);
    if (!picker) return;
    const triggerLabel = picker.querySelector('.skyrewrite-picker-trigger span');
    const optionHost = picker.querySelector('.skyrewrite-picker-options');
    const search = picker.querySelector('.skyrewrite-picker-search input');
    const selected = options.find((option) => option.value === String(selectedValue)) || options[0];
    if (triggerLabel) triggerLabel.textContent = selected?.label || '';
    if (!optionHost) return;
    const needle = (search?.value || '').trim().toLocaleLowerCase();
    optionHost.replaceChildren();
    const matches = options.filter((option) => option.label.toLocaleLowerCase().includes(needle));
    if (!matches.length) {
      const empty = document.createElement('div'); empty.className = 'skyrewrite-picker-empty'; empty.textContent = 'No matching filters'; optionHost.appendChild(empty);
      return;
    }
    for (const option of matches) {
      const button = document.createElement('button');
      button.type = 'button'; button.className = 'skyrewrite-picker-option'; button.dataset.value = option.value;
      button.setAttribute('role', 'option'); button.setAttribute('aria-selected', option.value === String(selectedValue) ? 'true' : 'false');
      if (option.value === String(selectedValue)) button.classList.add('selected');
      button.textContent = option.label;
      optionHost.appendChild(button);
    }
  }

  function getSkyRewritePickerOptions(name) {
    if (name.startsWith('achievement-')) return state.skyAchievementsPickerOptions[name] || [{ value: 'all', label: name === 'achievement-item-rarity' ? 'All rarities' : 'All plugins' }];
    if (name === 'collection-rarity') return [
      { value: 'all', label: 'All rarities' }, { value: 'recent', label: 'Recent discoveries' },
      { value: 'Common', label: 'Common' }, { value: 'Uncommon', label: 'Uncommon' },
      { value: 'Rare', label: 'Rare' }, { value: 'Epic', label: 'Epic' }, { value: 'Legendary', label: 'Legendary' }
    ];
    if (name === 'collection-plugin') {
      const plugins = [...new Set([...state.skycollectionItems, ...state.skycollectionLocations].map((item) => item.plugin).filter(Boolean))].sort((a, b) => a.localeCompare(b));
      return [{ value: 'all', label: 'All plugins' }, ...plugins.map((plugin) => ({ value: plugin, label: plugin }))];
    }
    if (name === 'actor-grouping') return [{ value: 'type', label: 'Actor type' }, { value: 'reference', label: 'Placed reference' }];
    if (name === 'actor-sort') return [{ value: 'kills', label: 'Most kills' }, { value: 'recent', label: 'Recently seen' }, { value: 'locations', label: 'Most locations' }, { value: 'name', label: 'Name A–Z' }];
    if (name === 'pressure-sort') return [{ value: 'pressure', label: 'Most pressure' }, { value: 'recent', label: 'Most recent' }, { value: 'name', label: 'Location A–Z' }];
    if (name === 'pressure-location') {
      const locations = [...new Set(state.skyrewriteEvents.filter((event) => event.kind === 'death' && event.detail === 'Player killed this actor').map((event) => event.location || 'Location unavailable'))].sort((a, b) => a.localeCompare(b));
      return [{ value: 'all', label: 'All locations' }, ...locations.map((location) => ({ value: location, label: location }))];
    }
    if (name === 'pressure-plugin') {
      const plugins = [...new Set(state.skyrewriteEvents.filter((event) => event.kind === 'death' && event.detail === 'Player killed this actor').map((event) => eventPluginOwner(event) || 'Plugin owner unavailable'))].sort((a, b) => a.localeCompare(b));
      return [{ value: 'all', label: 'All plugins' }, ...plugins.map((plugin) => ({ value: plugin === 'Plugin owner unavailable' ? '__unavailable__' : plugin, label: plugin }))];
    }
    if (name === 'actor-location') return state.skyrewriteActorLocationOptions;
    if (name === 'actor-plugin') return state.skyrewriteActorPluginOptions;
    if (name === 'location') {
      const locations = [...new Set(state.skyrewriteEvents.map((event) => event.location || 'Location unavailable'))].sort((a, b) => a.localeCompare(b));
      return [{ value: 'all', label: 'All locations' }, ...locations.map((location) => ({ value: location, label: location }))];
    }
    if (name === 'plugin') {
      const plugins = [...new Set(state.skyrewriteEvents.flatMap((event) => [eventPluginOwner(event), eventPluginOwner(event, 'base'), eventPluginOwner(event, 'location')]).filter(Boolean))].sort((a, b) => a.localeCompare(b));
      return [{ value: 'all', label: 'All plugins' }, ...plugins.map((plugin) => ({ value: plugin, label: plugin }))];
    }
    const options = [{ value: 'all', label: 'All retained' }, { value: '7', label: 'Last 7 in-game days' }, { value: '30', label: 'Last 30 in-game days' }];
    if (state.skyrewriteTimeRange !== 'all' && !['7', '30'].includes(String(state.skyrewriteTimeRange))) options.push({ value: String(state.skyrewriteTimeRange), label: `Last ${state.skyrewriteTimeRange} in-game days` });
    return options;
  }

  function getSkyRewritePickerValue(name) {
    if (name.startsWith('achievement-')) return state.skyAchievementsFilters[name] || 'all';
    if (name === 'collection-rarity') return state.skycollectionRarity;
    if (name === 'collection-plugin') return state.skycollectionPlugin;
    if (name === 'location') return state.skyrewriteLocationFilter;
    if (name === 'plugin') return state.skyrewritePluginFilter;
    if (name === 'actor-grouping') return state.skyrewriteActorGrouping;
    if (name === 'actor-sort') return state.skyrewriteActorSort;
    if (name === 'pressure-location') return state.skyrewritePressureLocation;
    if (name === 'pressure-plugin') return state.skyrewritePressurePlugin;
    if (name === 'pressure-sort') return state.skyrewritePressureSort;
    if (name === 'actor-location') return state.skyrewriteActorLocation;
    if (name === 'actor-plugin') return state.skyrewriteActorPlugin;
    return state.skyrewriteTimeRange;
  }

  function modLogoOrGlyph(src, className, glyph) {
    if (typeof src === 'string' && (/^data:image\/(png|jpeg|webp);base64,/i.test(src) || /^file:\/\/\//i.test(src))) {
      const image = document.createElement('img');
      image.className = `${className} mod-logo`;
      image.alt = '';
      image.loading = 'lazy';
      image.decoding = 'async';
      image.src = src;
      image.addEventListener('error', () => image.replaceWith(modLogoOrGlyph('', className, glyph)), { once: true });
      return image;
    }
    const fallback = document.createElement('span');
    fallback.className = className;
    fallback.textContent = glyph;
    return fallback;
  }

  const themeTextDefaults = {
    dark: '#e9edf2', light: '#302b22', cyberpunk: '#f4efff',
    'vampire-neon': '#f5eaf0', 'deep-blue': '#e8f3ff'
  };
  const supportedFonts = ['main', 'neon', 'dwemer', 'terminal', 'aspire', 'timeburner', 'balingkang'];
  const textPalette = ['#e9edf2', '#f0c77a', '#70d2df', '#8de0b5', '#b8a0ff', '#ff82bc', '#ff9b72', '#ffffff', '#9ca8b8', '#302b22'];

  function applyTextColor(color, persist = true) {
    if (!/^#[0-9a-f]{6}$/i.test(color || '')) return;
    document.documentElement.style.setProperty('--skyos-user-text', color);
    const picker = byId('text-color-choice');
    if (picker) picker.value = color;
    if (persist) {
      try { localStorage.setItem(`skyos-text-${document.documentElement.dataset.theme || 'dark'}`, color); } catch { /* Keep the choice for this session. */ }
    }
  }

  function applyTheme(theme) {
    const supported = ['dark', 'light', 'cyberpunk', 'vampire-neon', 'deep-blue'].includes(theme) ? theme : 'dark';
    document.documentElement.dataset.theme = supported;
    const selector = byId('theme-choice');
    if (selector) selector.value = supported;
    let savedColor = '';
    try {
      localStorage.setItem('skyos-theme', supported);
      savedColor = localStorage.getItem(`skyos-text-${supported}`) || '';
    } catch { /* Keep the choice for this session. */ }
    applyTextColor(savedColor || themeTextDefaults[supported], false);
  }

  function applyFont(font, persist = true) {
    const supported = supportedFonts.includes(font) ? font : 'main';
    document.documentElement.dataset.font = supported;
    const selector = byId('font-choice');
    if (selector) selector.value = supported;
    if (persist) {
      try { localStorage.setItem('skyos-font', supported); } catch { /* Keep the choice for this session. */ }
    }
  }

  let initialTheme = 'dark';
  let initialFont = 'main';
  try {
    initialTheme = localStorage.getItem('skyos-theme') || 'dark';
    initialFont = localStorage.getItem('skyos-font') || 'main';
  } catch { /* Use the default appearance for this session. */ }
  document.documentElement.dataset.theme = initialTheme;
  document.documentElement.dataset.font = initialFont;


  function skyrewriteSettings() {
    const values = state.settings || defaultSkyRewriteSettings;
    return {
      historyRefreshInterval: Math.max(250, Math.min(30000, Number(values.historyRefreshInterval ?? 3000) || 3000)),
      showLocationSummary: values.showLocationSummary !== false,
      showInventoryEvents: values.showInventoryEvents !== false,
      localEchoKillThreshold: Math.max(1, Math.min(20, Math.round(Number(values.localEchoKillThreshold ?? 3) || 3))),
      localEchoLookbackDays: Math.max(0.5, Math.min(60, Number(values.localEchoLookbackDays ?? 7) || 7)),
      localEchoCooldownDays: Math.max(0.5, Math.min(60, Number(values.localEchoCooldownDays ?? 7) || 7)),
      localEchoEnabled: values.localEchoEnabled === true,
      localEchoShowNotification: values.localEchoShowNotification !== false,
      adaptiveGuardResponse: values.adaptiveGuardResponse === true,
      localEchoTemporaryBounty: values.localEchoTemporaryBounty === true,
      localEchoTemporaryBountyAmount: Math.max(5, Math.min(100, Number(values.localEchoTemporaryBountyAmount ?? 25) || 25)),
      localEchoTemporaryBountyDays: Math.max(0.5, Math.min(7, Number(values.localEchoTemporaryBountyDays ?? 1) || 1)),
      localEchoTemporaryBountyScaleWithPressure: values.localEchoTemporaryBountyScaleWithPressure === true,
      bountyVictimMode: ['nonhostile', 'hostile', 'both'].includes(String(values.echoBountyVictimMode || 'nonhostile')) ? String(values.echoBountyVictimMode || 'nonhostile') : 'nonhostile',
      theftBountyEnabled: values.theftBountyEnabled === true,
      theftBountyAmount: Math.max(5, Math.min(100, Number(values.theftBountyAmount ?? 25) || 25)),
      theftBountyThreshold: Math.max(1, Math.min(100, Math.round(Number(values.theftBountyThreshold ?? 3) || 3))),
      dispatchGuardOnBounty: values.dispatchGuardOnBounty !== false,
      journalRetentionEvents: Math.max(512, Math.min(2048, Number(values.journalRetentionEvents ?? 2048) || 2048))
    };
  }

  function applySkyRewriteSettings() {
    const values = skyrewriteSettings();
    invoke('skyos_skyrewrite_local_echo', values.localEchoEnabled ? '1' : '0');
    invoke('skyos_skyrewrite_echo_notification', values.localEchoShowNotification ? '1' : '0');
    invoke('skyos_skyrewrite_adaptive_guard_response', values.adaptiveGuardResponse ? '1' : '0');
    invoke('skyos_skyrewrite_temporary_bounty', values.localEchoTemporaryBounty ? '1' : '0');
    invoke('skyos_skyrewrite_temporary_bounty_amount', String(values.localEchoTemporaryBountyAmount));
    invoke('skyos_skyrewrite_temporary_bounty_days', String(values.localEchoTemporaryBountyDays));
    invoke('skyos_skyrewrite_temporary_bounty_pressure_scaling', values.localEchoTemporaryBountyScaleWithPressure ? '1' : '0');
    invoke('skyos_skyrewrite_bounty_victim_mode', values.bountyVictimMode);
    invoke('skyos_skyrewrite_theft_bounty', values.theftBountyEnabled ? '1' : '0');
    invoke('skyos_skyrewrite_theft_bounty_amount', String(values.theftBountyAmount));
    invoke('skyos_skyrewrite_theft_bounty_threshold', String(values.theftBountyThreshold));
    invoke('skyos_skyrewrite_dispatch_guard_on_bounty', values.dispatchGuardOnBounty ? '1' : '0');
    invoke('skyos_skyrewrite_local_echo_rules', `${values.localEchoKillThreshold}|${values.localEchoLookbackDays}|${values.localEchoCooldownDays}`);
    invoke('skyos_skyrewrite_journal_retention', String(values.journalRetentionEvents));
    SkyRewriteRenderer.setSkyRewriteHistory();
  }

  function setSkyRewriteVisibility(visible) {
    const nextVisible = visible === true;
    if (state.skyrewriteVisible === nextVisible) return;
    state.skyrewriteVisible = nextVisible;
    if (state.skyrewriteTimer) window.clearTimeout(state.skyrewriteTimer);
    state.skyrewriteTimer = 0;
    if (nextVisible) {
      reloadSkyRewriteHistory();
      if (state.skyrewriteTab === 'bounties') invoke('skyos_skyrewrite_game_bounties');
    }
  }

  function saveSkyRewriteSettings() {
    try { localStorage.setItem('skyrewrite-settings-v1', JSON.stringify(state.settings)); }
    catch (error) { console.warn('SkyRewrite could not save its settings', error); }
    applySkyRewriteSettings();
  }

const SkyRewriteRenderer = {
    setSkyRewriteHistory(events) {
      const host = byId('skyrewrite-history');
      const count = byId('skyrewrite-event-count');
      const locationsHost = byId('skyrewrite-location-summary');
      if (!host || !count) return;
      const settings = skyrewriteSettings();
      const memoryConfig = byId('skyrewrite-memory-config');
      if (memoryConfig) memoryConfig.textContent = `${settings.localEchoEnabled ? 'Local Echo enabled' : 'Local Echo disabled'} · pressure threshold ${settings.localEchoKillThreshold} within ${settings.localEchoLookbackDays} in-game days · ${settings.localEchoCooldownDays}-day per-location cooldown · HUD notice ${settings.localEchoShowNotification ? 'on' : 'off'} · adaptive guards ${settings.adaptiveGuardResponse ? 'on' : 'off'} · Echo bounty targets ${settings.bountyVictimMode} · theft bounty ${settings.theftBountyEnabled ? `on (${settings.theftBountyAmount} septims after ${settings.theftBountyThreshold} thefts)` : 'off'} · guard dispatch ${settings.dispatchGuardOnBounty ? 'on' : 'off'} · temporary Echo bounty ${settings.localEchoTemporaryBounty ? `on (${settings.localEchoTemporaryBountyAmount} septims${settings.localEchoTemporaryBountyScaleWithPressure ? ', pressure-scaled' : ''}, ${settings.localEchoTemporaryBountyDays} days)` : 'off'} · journal cap ${settings.journalRetentionEvents}`;
      const summaryPanel = locationsHost?.closest('.skyrewrite-panel');
      if (summaryPanel) summaryPanel.classList.toggle('hidden', !settings.showLocationSummary);
      const inventoryFilter = document.querySelector('[data-skyrewrite-filter="inventory"]');
      if (inventoryFilter) inventoryFilter.classList.toggle('hidden', !settings.showInventoryEvents);
      if (Array.isArray(events)) { state.skyrewriteEvents = events; state.skyrewriteCurrentDay = null; state.skyrewriteRecorder = null; state.skyrewriteRegionalBanditKills = null; state.skyrewriteRegionalBanditStates = []; state.skyrewriteRegionalBanditHoldStates = []; state.skyrewriteRegionalCreatureStates = []; state.skyrewriteRegionalFavorStates = []; state.skyrewriteReputation = null; state.skyrewriteGameBounties = [];  }
      else if (events && Array.isArray(events.events)) {
        const pageOffset = Math.max(0, Number(events.eventOffset) || 0);
        const pageCount = Math.max(0, Number(events.eventCount) || events.events.length);
        if (pageOffset === 0) {
          state.skyrewriteEvents = events.events;
          state.skyrewriteHistoryNextOffset = pageCount;
        } else if (pageOffset === state.skyrewriteHistoryNextOffset) {
          state.skyrewriteEvents = state.skyrewriteEvents.concat(events.events);
          state.skyrewriteHistoryNextOffset = pageOffset + pageCount;
        }
        state.skyrewriteHistoryTotal = Math.max(0, Number(events.eventTotal) || 0);
        state.skyrewriteHistoryHasMore = events.hasMoreEvents === true;
        state.skyrewriteHistoryLoading = false;
        const currentDay = events.currentDay !== null && events.currentDay !== undefined && Number.isFinite(Number(events.currentDay)) ? Number(events.currentDay) : null;
        const recorder = { installed: events.recorderInstalled === true, captured: Number(events.sourceEventsThisSession) || 0, deathsSeen: Number(events.deathEventsSeenThisSession) || 0, playerKills: Number(events.playerKillsCapturedThisSession) || 0, retainedKills: Number(events.retainedPlayerKills) || 0, retainedEchoes: Number(events.retainedEchoes) || 0 };
        state.skyrewriteCurrentDay = currentDay;
        state.skyrewriteRecorder = recorder;
        state.skyrewriteRegionalBanditKills = Number.isFinite(Number(events.regionalBanditKills)) ? Number(events.regionalBanditKills) : null;
        state.skyrewriteRegionalBanditStates = Array.isArray(events.regionalBanditStates) ? events.regionalBanditStates : [];
        state.skyrewriteRegionalBanditHoldStates = Array.isArray(events.regionalBanditHoldStates) ? events.regionalBanditHoldStates : [];
        state.skyrewriteRegionalCreatureStates = Array.isArray(events.regionalCreatureStates) ? events.regionalCreatureStates : [];
        state.skyrewriteRegionalFavorStates = Array.isArray(events.regionalFavorStates) ? events.regionalFavorStates : [];
        state.skyrewriteReputation = events.reputation && typeof events.reputation === 'object' ? events.reputation : null;
        state.skyrewriteGameBounties = Array.isArray(events.gameBounties) ? events.gameBounties : [];
        
      }
      if (state.activePage !== 'skyrewrite') return;
      const items = state.skyrewriteEvents;
      if (state.skyrewriteTab === 'history') {
        const availableLocations = [...new Set(items.map((event) => event.location || 'Location unavailable'))].sort((a, b) => a.localeCompare(b));
        state.skyrewriteLocationFilter = state.skyrewriteLocationFilter === 'all' || availableLocations.includes(state.skyrewriteLocationFilter) ? state.skyrewriteLocationFilter : 'all';
        renderSkyRewritePicker('location', [{ value: 'all', label: 'All locations' }, ...availableLocations.map((location) => ({ value: location, label: location }))], state.skyrewriteLocationFilter);
        const availablePlugins = [...new Set(items.flatMap((event) => [eventPluginOwner(event), eventPluginOwner(event, 'base'), eventPluginOwner(event, 'location')]).filter(Boolean))].sort((a, b) => a.localeCompare(b));
        state.skyrewritePluginFilter = state.skyrewritePluginFilter === 'all' || availablePlugins.includes(state.skyrewritePluginFilter) ? state.skyrewritePluginFilter : 'all';
        renderSkyRewritePicker('plugin', [{ value: 'all', label: 'All plugins' }, ...availablePlugins.map((plugin) => ({ value: plugin, label: plugin }))], state.skyrewritePluginFilter);
        const timeOptions = [{ value: 'all', label: 'All retained' }, { value: '7', label: 'Last 7 in-game days' }, { value: '30', label: 'Last 30 in-game days' }];
        if (state.skyrewriteTimeRange !== 'all' && !['7', '30'].includes(String(state.skyrewriteTimeRange))) {
          timeOptions.push({ value: String(state.skyrewriteTimeRange), label: `Last ${state.skyrewriteTimeRange} in-game days` });
        }
        renderSkyRewritePicker('time', timeOptions, state.skyrewriteTimeRange);
        const customDays = byId('skyrewrite-custom-days');
        if (customDays && document.activeElement !== customDays) customDays.value = !['all', '7', '30'].includes(String(state.skyrewriteTimeRange)) ? String(state.skyrewriteTimeRange) : '';
        renderSkyRewriteHistoryList(items, settings);
        return;
      }
      renderSkyRewriteRegionalTrade(items);
      renderSkyRewriteTheftHistory(items);
      renderSkyRewriteReputation();
      renderSkyRewriteGameBountyLedger(state.skyrewriteGameBounties, items);
      renderSkyRewriteBountyLedger(items, state.skyrewriteCurrentDay);
      const bountySummary = byId('skyrewrite-bounties-count');
      if (bountySummary) {
        const resolutions = new Set(items.filter((event) => Number(event.eventType) === 7).map((event) => String(event.identity || '')));
        const pendingSkyRewrite = items.filter((event) => Number(event.eventType) === 6 && !resolutions.has(String(event.identity || ''))).length;
        bountySummary.textContent = `${state.skyrewriteGameBounties.length} Skyrim hold${state.skyrewriteGameBounties.length === 1 ? '' : 's'} · ${pendingSkyRewrite} active SkyRewrite`;
      }
      if (state.skyrewriteTab === 'echo') renderSkyRewriteEchoOutcomes(items);
      const actorHost = byId('skyrewrite-actor-memory');
      if (state.skyrewriteTab === 'npc' && actorHost) {
        const actors = new Map();
        for (const event of items) {
          if (event.kind !== 'death' || event.detail !== 'Player killed this actor') continue;
          const referenceID = Number(event.formID) >>> 0;
          const baseFormID = Number(event.baseFormID) >>> 0;
          const byType = state.skyrewriteActorGrouping !== 'reference';
          const groupID = byType ? (baseFormID || referenceID) : (referenceID || baseFormID);
          const formIDText = groupID.toString(16).toUpperCase().padStart(8, '0');
          const referenceIDText = referenceID.toString(16).toUpperCase().padStart(8, '0');
          const key = groupID ? `${byType && baseFormID ? 'base' : 'reference'}:${groupID}` : `${event.subject || 'Unknown actor'}:${event.location || ''}`;
          if (!actors.has(key)) actors.set(key, { baseFormID, basePlugin: eventPluginOwner(event, 'base'), referenceID, referencePlugin: eventPluginOwner(event, 'form'), referenceIDText, formIDText, name: event.subject || 'Unknown actor', count: 0, hostileKills: 0, nonHostileKills: 0, unknownDisposition: 0, guardVictim: false, essentialVictim: false, teammateVictim: false, references: new Set(), referenceStats: new Map(), locations: new Set(), firstDay: null, lastDay: null, location: event.location || 'Location unavailable' });
          const actor = actors.get(key);
          actor.baseFormID ||= baseFormID;
          actor.basePlugin ||= eventPluginOwner(event, 'base');
          actor.referencePlugin ||= eventPluginOwner(event, 'form');
          actor.count++;
          if (event.victimHostile === true) actor.hostileKills++;
          else if (event.victimHostile === false) actor.nonHostileKills++;
          else actor.unknownDisposition++;
          actor.guardVictim ||= event.victimGuard === true;
          actor.essentialVictim ||= event.victimEssential === true;
          actor.teammateVictim ||= event.victimTeammate === true;
          if (referenceID) {
            actor.references.add(referenceID);
            if (!actor.referenceStats.has(referenceID)) actor.referenceStats.set(referenceID, { id: referenceID, plugin: eventPluginOwner(event, 'form'), kills: 0, locations: new Set(), lastDay: null });
            const reference = actor.referenceStats.get(referenceID);
            reference.kills++;
            reference.plugin ||= eventPluginOwner(event, 'form');
            if (event.location && event.location !== 'Location unavailable') reference.locations.add(event.location);
            if (event.day != null && Number.isFinite(Number(event.day))) reference.lastDay = reference.lastDay === null ? Number(event.day) : Math.max(reference.lastDay, Number(event.day));
          }
          if (event.location && event.location !== 'Location unavailable') actor.locations.add(event.location);
          if (event.day != null && Number.isFinite(Number(event.day)) && (actor.lastDay === null || Number(event.day) >= actor.lastDay)) {
            actor.lastDay = Number(event.day);
            actor.location = event.location || 'Location unavailable';
            actor.name = event.subject || actor.name;
            actor.referenceID = referenceID;
            actor.referenceIDText = referenceIDText;
          }
          if (event.day != null && Number.isFinite(Number(event.day)) && (actor.firstDay === null || Number(event.day) < actor.firstDay)) actor.firstDay = Number(event.day);
        }
        const allActors = [...actors.values()];
        const actorNeedle = state.skyrewriteActorSearch.trim().toLocaleLowerCase();
        renderSkyRewritePicker('actor-grouping', getSkyRewritePickerOptions('actor-grouping'), state.skyrewriteActorGrouping);
        renderSkyRewritePicker('actor-sort', getSkyRewritePickerOptions('actor-sort'), state.skyrewriteActorSort);
        const actorLocationOptions = [{ value: 'all', label: 'All locations' }, ...[...new Set(allActors.flatMap((actor) => actor.locations.size ? [...actor.locations] : ['Location unavailable']))].sort((a, b) => a.localeCompare(b)).map((location) => ({ value: location === 'Location unavailable' ? '__unavailable__' : location, label: location }))];
        const actorPluginFor = (actor) => state.skyrewriteActorGrouping === 'reference' ? actor.referencePlugin : actor.basePlugin;
        const actorOwners = allActors.map(actorPluginFor);
        const actorPluginOptions = [{ value: 'all', label: 'All plugins' }, ...[...new Set(actorOwners.filter(Boolean))].sort((a, b) => a.localeCompare(b)).map((plugin) => ({ value: plugin, label: plugin }))];
        if (actorOwners.some((owner) => !owner)) actorPluginOptions.push({ value: '__unavailable__', label: 'Plugin owner unavailable' });
        state.skyrewriteActorLocationOptions = actorLocationOptions;
        state.skyrewriteActorPluginOptions = actorPluginOptions;
        if (!actorLocationOptions.some((option) => option.value === state.skyrewriteActorLocation)) state.skyrewriteActorLocation = 'all';
        if (!actorPluginOptions.some((option) => option.value === state.skyrewriteActorPlugin)) state.skyrewriteActorPlugin = 'all';
        renderSkyRewritePicker('actor-location', actorLocationOptions, state.skyrewriteActorLocation);
        renderSkyRewritePicker('actor-plugin', actorPluginOptions, state.skyrewriteActorPlugin);
        const visibleActors = allActors.filter((actor) => {
          const matchesFilter = state.skyrewriteActorFilter === 'all' || (state.skyrewriteActorFilter === 'recurring' ? actor.count > 1 : state.skyrewriteActorFilter === 'multi-location' ? actor.locations.size > 1 : actor.count === 1);
          const actorLocations = actor.locations.size ? [...actor.locations] : ['Location unavailable'];
          const matchesLocation = state.skyrewriteActorLocation === 'all' || actorLocations.includes(state.skyrewriteActorLocation === '__unavailable__' ? 'Location unavailable' : state.skyrewriteActorLocation);
          const actorPlugin = actorPluginFor(actor);
          const matchesPlugin = state.skyrewriteActorPlugin === 'all' || (state.skyrewriteActorPlugin === '__unavailable__' ? !actorPlugin : actorPlugin === state.skyrewriteActorPlugin);
          const firstDay = actor.firstDay === null ? '' : `day ${actor.firstDay.toFixed(1)} ${actor.firstDay.toFixed(1)}`;
          const lastDay = actor.lastDay === null ? '' : `day ${actor.lastDay.toFixed(1)} ${actor.lastDay.toFixed(1)}`;
          const baseIDText = Number(actor.baseFormID || 0).toString(16).toUpperCase().padStart(8, '0');
          const searchable = `${actor.name} ${actor.formIDText} ${actor.referenceIDText} ${baseIDText} ${[...actor.locations].join(' ')} ${actor.basePlugin} ${actor.referencePlugin} ${firstDay} ${lastDay}`.toLocaleLowerCase();
          return matchesFilter && matchesLocation && matchesPlugin && searchable.includes(actorNeedle);
        });
        visibleActors.sort((a, b) => state.skyrewriteActorSort === 'name' ? a.name.localeCompare(b.name) : state.skyrewriteActorSort === 'recent' ? (b.lastDay ?? -Infinity) - (a.lastDay ?? -Infinity) || a.name.localeCompare(b.name) : state.skyrewriteActorSort === 'locations' ? b.locations.size - a.locations.size || b.count - a.count : b.count - a.count || (b.lastDay ?? -Infinity) - (a.lastDay ?? -Infinity) || a.name.localeCompare(b.name));
        const actorCount = byId('skyrewrite-actor-count');
        if (actorCount) actorCount.textContent = `${allActors.length} ${state.skyrewriteActorGrouping === 'reference' ? 'placed references' : 'actor types'} · ${allActors.reduce((total, actor) => total + actor.count, 0)} recorded kills`;
        document.querySelectorAll('[data-skyrewrite-actor-filter]').forEach((button) => {
          const active = button.dataset.skyrewriteActorFilter === state.skyrewriteActorFilter;
          button.classList.toggle('active', active);
          button.setAttribute('aria-pressed', String(active));
        });
        actorHost.replaceChildren();
        if (!visibleActors.length) {
          const empty = document.createElement('div'); empty.className = 'empty';
          empty.textContent = allActors.length ? 'No actor groups match this search and filter.' : 'No player-caused kills are recorded in the retained journal yet.';
          actorHost.appendChild(empty);
        } else {
          for (const actor of visibleActors) {
            const row = document.createElement('article'); row.className = 'skyrewrite-actor-row';
            const identity = document.createElement('div'); identity.className = 'skyrewrite-actor-identity';
            const title = document.createElement('strong'); title.textContent = actor.name;
            const actorOwner = state.skyrewriteActorGrouping === 'reference'
              ? actor.referencePlugin
              : actor.basePlugin;
            const formIsDynamic = ((actor.baseFormID || actor.referenceID) >>> 24) === 0xFF;
            const ownerLabel = actorOwner || (formIsDynamic ? 'runtime/save-created form' : 'plugin owner unavailable');
            const baseIDText = Number(actor.baseFormID || 0).toString(16).toUpperCase().padStart(8, '0');
            const id = document.createElement('small');
            id.textContent = state.skyrewriteActorGrouping === 'reference'
              ? `Placed reference · ${actor.formIDText} · ${ownerLabel} · actor base ${actor.baseFormID ? baseIDText : 'unavailable'}`
              : actor.baseFormID ? `Actor base · ${actor.formIDText} · ${ownerLabel} · ${actor.references.size} placed references` : `Actor reference · ${actor.formIDText} · ${ownerLabel} · base ID unavailable`;
            identity.append(title, id);
            const reactionMemory = document.createElement('small');
            const knownKills = actor.hostileKills + actor.nonHostileKills;
            const dispositions = knownKills
              ? `Recorded victim context · ${actor.hostileKills} hostile · ${actor.nonHostileKills} non-hostile${actor.unknownDisposition ? ` · ${actor.unknownDisposition} older/unknown` : ''}`
              : 'Victim disposition unavailable for retained events';
            const safeguards = [actor.guardVictim && 'guard', actor.essentialVictim && 'essential', actor.teammateVictim && 'teammate'].filter(Boolean);
            reactionMemory.textContent = safeguards.length ? `${dispositions} · flagged ${safeguards.join(', ')}` : dispositions;
            reactionMemory.className = 'skyrewrite-actor-memory-context';
            identity.appendChild(reactionMemory);
            const countBadge = document.createElement('span'); countBadge.className = `skyrewrite-actor-count${actor.count > 1 ? ' recurring' : ''}`; countBadge.textContent = `${actor.count} ${actor.count === 1 ? 'kill' : 'kills'}`;
            const expandedKey = `${state.skyrewriteActorGrouping}:${actor.formIDText}`;
            const isExpanded = state.skyrewriteActorExpanded.has(expandedKey);
            const breakdownButton = document.createElement('button'); breakdownButton.type = 'button'; breakdownButton.className = 'skyrewrite-memory-history'; breakdownButton.textContent = isExpanded ? 'Hide details' : 'Details'; breakdownButton.setAttribute('aria-expanded', String(isExpanded));
            const breakdown = document.createElement('div'); breakdown.className = 'skyrewrite-actor-breakdown'; breakdown.hidden = !isExpanded;
            const references = [...actor.referenceStats.values()].sort((a, b) => b.kills - a.kills || a.id - b.id);
            if (!references.length) {
              const unavailable = document.createElement('small'); unavailable.textContent = 'Reference details were not recorded for these events.'; breakdown.appendChild(unavailable);
            } else {
              for (const reference of references) {
                const referenceRow = document.createElement('div'); referenceRow.className = 'skyrewrite-actor-reference';
                const referenceID = reference.id.toString(16).toUpperCase().padStart(8, '0');
                const referenceOwner = reference.plugin || (((reference.id >>> 24) & 0xFF) === 0xFF ? 'runtime/save-created form' : 'plugin owner unavailable');
                const label = document.createElement('strong'); label.textContent = `Reference ${referenceID}`;
                const details = document.createElement('small'); details.textContent = `${referenceOwner} · ${reference.kills} ${reference.kills === 1 ? 'kill' : 'kills'} · ${reference.locations.size ? [...reference.locations].sort((a, b) => a.localeCompare(b)).join(', ') : 'Location unavailable'} · ${reference.lastDay === null ? 'day unknown' : `last day ${reference.lastDay.toFixed(1)}`}`;
                referenceRow.append(label, details); breakdown.appendChild(referenceRow);
              }
            }
            breakdownButton.addEventListener('click', () => {
              breakdown.hidden = !breakdown.hidden;
              if (breakdown.hidden) state.skyrewriteActorExpanded.delete(expandedKey);
              else state.skyrewriteActorExpanded.add(expandedKey);
              breakdownButton.setAttribute('aria-expanded', String(!breakdown.hidden));
              breakdownButton.textContent = breakdown.hidden ? 'Details' : 'Hide details';
            });
            const last = document.createElement('small'); last.className = 'skyrewrite-actor-last'; last.textContent = `${actor.lastDay === null ? 'Day unknown' : `Last recorded · day ${actor.lastDay.toFixed(1)}`} · ${actor.location} · reference ${actor.referenceIDText}`;
            const span = document.createElement('small'); span.className = 'skyrewrite-actor-span';
            const first = actor.firstDay === null ? 'Day unknown' : `day ${actor.firstDay.toFixed(1)}`;
            const latest = actor.lastDay === null ? 'day unknown' : `day ${actor.lastDay.toFixed(1)}`;
            span.textContent = `${actor.locations.size ? `${actor.locations.size} ${actor.locations.size === 1 ? 'location' : 'locations'}` : 'Location unavailable'} recorded · first ${first} · latest ${latest}`;
            const open = document.createElement('button'); open.type = 'button'; open.className = 'skyrewrite-memory-history'; open.textContent = 'View events';
            open.addEventListener('click', () => {
              state.skyrewriteSearch = state.skyrewriteActorGrouping !== 'reference' && actor.baseFormID ? `Base FormID ${actor.formIDText}` : `FormID ${actor.formIDText}`;
              state.skyrewriteLocationFilter = 'all'; state.skyrewritePluginFilter = 'all'; state.skyrewriteTimeRange = 'all'; state.skyrewriteFilter = 'all';
              const historySearch = byId('skyrewrite-search-input'); if (historySearch) historySearch.value = state.skyrewriteSearch;
              setSkyRewriteTab('history');
              SkyRewriteRenderer.setSkyRewriteHistory();
              byId('skyrewrite-history')?.scrollIntoView({ behavior: 'smooth', block: 'center' });
            });
            const meta = document.createElement('div'); meta.className = 'skyrewrite-actor-meta'; meta.append(countBadge, breakdownButton, open);
            row.append(identity, meta, last, span, breakdown); actorHost.appendChild(row);
          }
        }
      }
      // Refresh combat traces and local pressure on every journal snapshot,
      // including while hidden, so tab changes cannot leave either view stale.
      const memoryHost = byId('skyrewrite-place-memory');
      if (memoryHost) {
        try {
        const latestRecordedDay = latestSkyRewriteEventDay(items, state.skyrewriteCurrentDay);
        const memory = new Map();
        for (const event of items) {
          const location = event.location || (isSkyRewriteEchoEvent(event) ? event.subject : '') || 'Location unavailable';
          if (!memory.has(location)) memory.set(location, { kills: 0, pressure: 0, lastKillDay: null, lastEchoDay: null, echoEvent: null, recordedPressure: 0 });
          const entry = memory.get(location);
          const day = event.day == null || event.day === '' ? null : Number(event.day);
          if (isSkyRewritePlayerKill(event) && Number.isFinite(day)) {
            const age = latestRecordedDay === null ? 0 : latestRecordedDay - day;
            if (age >= 0 && age < settings.localEchoLookbackDays) {
              entry.kills++;
              entry.pressure += Math.max(0, 1 - age / settings.localEchoLookbackDays);
              entry.lastKillDay = entry.lastKillDay === null ? day : Math.max(entry.lastKillDay, day);
            }
          } else if (isSkyRewriteEchoEvent(event)) {
            if (!entry.echoEvent || (hasSkyRewriteEventDay(event) && (!hasSkyRewriteEventDay(entry.echoEvent) || day >= Number(entry.echoEvent.day)))) entry.echoEvent = event;
            if (Number.isFinite(day)) entry.lastEchoDay = entry.lastEchoDay === null ? day : Math.max(entry.lastEchoDay, day);
            const recordedPressure = Number(String(event.detail || '').match(/pressure\s+([\d.]+)/i)?.[1]);
            if (Number.isFinite(recordedPressure)) entry.recordedPressure = Math.max(entry.recordedPressure, recordedPressure);
          }
        }
        memoryHost.replaceChildren();
        const entries = [...memory.entries()].filter(([, entry]) => entry.pressure > 0 || entry.echoEvent || (entry.lastEchoDay !== null && latestRecordedDay !== null && latestRecordedDay - entry.lastEchoDay <= settings.localEchoCooldownDays));
        const visibleEntries = entries.filter(([location, entry]) => {
          const cooldownLeft = entry.lastEchoDay === null || latestRecordedDay === null ? 0 : Math.max(0, settings.localEchoCooldownDays - (latestRecordedDay - entry.lastEchoDay));
          const displayPressure = Math.max(entry.pressure, entry.recordedPressure);
          const status = cooldownLeft > 0 ? 'cooldown' : displayPressure >= settings.localEchoKillThreshold ? 'ready' : 'building';
          return (state.skyrewriteMemoryFilter === 'all' || state.skyrewriteMemoryFilter === status) && location.toLocaleLowerCase().includes(state.skyrewriteMemorySearch.trim().toLocaleLowerCase());
        }).sort((a, b) => b[1].pressure - a[1].pressure || String(a[0]).localeCompare(String(b[0])));
        const memoryCount = byId('skyrewrite-memory-count');
        if (memoryCount) memoryCount.textContent = `${visibleEntries.length} of ${entries.length} locations`;
        document.querySelectorAll('[data-skyrewrite-memory-filter]').forEach((button) => {
          const active = button.dataset.skyrewriteMemoryFilter === state.skyrewriteMemoryFilter;
          button.classList.toggle('active', active);
          button.setAttribute('aria-pressed', String(active));
        });
        if (!entries.length) {
          const empty = document.createElement('div'); empty.className = 'empty';
          empty.textContent = 'No recent player-kill activity is recorded in the Local Echo window.';
          memoryHost.appendChild(empty);
        } else if (!visibleEntries.length) {
          const empty = document.createElement('div'); empty.className = 'empty';
          empty.textContent = 'No locations match this search and status filter.';
          memoryHost.appendChild(empty);
        } else {
          for (const [location, entry] of visibleEntries) {
            const row = document.createElement('article'); row.className = 'skyrewrite-memory-row';
            const rowCooldownLeft = entry.lastEchoDay === null || latestRecordedDay === null ? 0 : Math.max(0, settings.localEchoCooldownDays - (latestRecordedDay - entry.lastEchoDay));
            const rowPressure = Math.max(entry.pressure, entry.recordedPressure);
            row.dataset.skyrewriteMemoryStatus = rowCooldownLeft > 0 ? 'cooldown' : rowPressure >= settings.localEchoKillThreshold ? 'ready' : 'building';
            const heading = document.createElement('div'); heading.className = 'skyrewrite-memory-heading';
            const name = document.createElement('strong'); name.textContent = location;
            const stateLabel = document.createElement('span'); stateLabel.className = 'skyrewrite-memory-state';
            const cooldownLeft = entry.lastEchoDay === null || latestRecordedDay === null ? 0 : Math.max(0, settings.localEchoCooldownDays - (latestRecordedDay - entry.lastEchoDay));
            const thresholdMet = rowPressure >= settings.localEchoKillThreshold;
            stateLabel.textContent = cooldownLeft > 0 ? `Cooling down · ${cooldownLeft.toFixed(1)} days` : entry.echoEvent && entry.pressure === 0 ? `Echo fired · ${entry.recordedPressure.toFixed(2)} pressure` : thresholdMet ? (settings.localEchoEnabled ? `Ready · ${entry.pressure.toFixed(2)} pressure` : `Threshold met · Echo disabled`) : `Building · ${entry.pressure.toFixed(2)} / ${settings.localEchoKillThreshold} pressure`;
            if (cooldownLeft > 0) stateLabel.classList.add('cooldown');
            else if (thresholdMet) stateLabel.classList.add('ready');
            heading.append(name, stateLabel);
            const progress = document.createElement('div'); progress.className = 'skyrewrite-memory-track';
            const fill = document.createElement('span'); fill.style.width = `${Math.min(100, Math.max(entry.pressure, entry.recordedPressure) / settings.localEchoKillThreshold * 100)}%`; progress.appendChild(fill);
            const meta = document.createElement('small');
            const echoKills = Number(String(entry.echoEvent?.detail || '').match(/from\s+(\d+)\s+kills/i)?.[1]) || Number(String(entry.echoEvent?.detail || '').match(/from\s+(\d+)\s+player kills/i)?.[1]) || Number(entry.echoEvent?.stage) || 0;
            const echoThefts = Number(String(entry.echoEvent?.detail || '').match(/and\s+(\d+)\s+thefts/i)?.[1]) || 0;
            meta.textContent = `${entry.lastKillDay === null || latestRecordedDay === null ? `${entry.echoEvent ? `${echoKills} kills · ${echoThefts} thefts recorded at Echo` : 'No recent recorded kills'}` : `Last kill · ${Math.max(0, latestRecordedDay - entry.lastKillDay).toFixed(1)} in-game days ago`} · ${entry.lastEchoDay === null ? (entry.echoEvent ? 'Echo day unavailable' : 'No prior echo') : `Last echo · day ${entry.lastEchoDay.toFixed(1)}`}`;
            const viewHistory = document.createElement('button'); viewHistory.type = 'button'; viewHistory.className = 'skyrewrite-memory-history'; viewHistory.textContent = 'View history';
            viewHistory.addEventListener('click', () => {
              state.skyrewriteLocationFilter = location;
              setSkyRewriteTab('history');
              SkyRewriteRenderer.setSkyRewriteHistory();
              byId('skyrewrite-history')?.scrollIntoView({ behavior: 'smooth', block: 'center' });
            });
            const footer = document.createElement('div'); footer.className = 'skyrewrite-memory-footer'; footer.append(meta, viewHistory);
            row.append(heading, progress, footer); memoryHost.appendChild(row);
          }
        }
        } catch (error) {
          console.error('SkyRewrite place memory render failed', error);
          const detail = String(error?.message || error || 'unknown error').slice(0, 180);
          const empty = document.createElement('div'); empty.className = 'empty'; empty.textContent = `Place memory could not render: ${detail}`;
          memoryHost.replaceChildren(empty);
        }
      }
      const pressureHost = byId('skyrewrite-pressure-preview');
      if (pressureHost) {
        try {
        const pressureLocationOptions = getSkyRewritePickerOptions('pressure-location');
        const pressurePluginOptions = getSkyRewritePickerOptions('pressure-plugin');
        if (!pressureLocationOptions.some((option) => option.value === state.skyrewritePressureLocation)) state.skyrewritePressureLocation = 'all';
        if (!pressurePluginOptions.some((option) => option.value === state.skyrewritePressurePlugin)) state.skyrewritePressurePlugin = 'all';
        const latestPressureDay = latestSkyRewriteEventDay(items, state.skyrewriteCurrentDay);
        const pressureByLocation = new Map();
        if (latestPressureDay !== null) {
          for (const event of items) {
            const isKill = isSkyRewritePlayerKill(event);
            const isTheft = isSkyRewriteTheft(event);
            if (!isKill && !isTheft) continue;
            const location = event.location || 'Location unavailable';
            const plugin = eventPluginOwner(event) || 'Plugin owner unavailable';
            if (state.skyrewritePressureLocation !== 'all' && location !== state.skyrewritePressureLocation) continue;
            if (state.skyrewritePressurePlugin !== 'all' && (state.skyrewritePressurePlugin === '__unavailable__' ? plugin !== 'Plugin owner unavailable' : plugin !== state.skyrewritePressurePlugin)) continue;
            if (event.day === null || event.day === undefined || event.day === '') continue;
            const day = Number(event.day);
            if (!Number.isFinite(day)) continue;
            const age = latestPressureDay - day;
            if (age < 0 || age >= settings.localEchoLookbackDays) continue;
            if (!pressureByLocation.has(location)) pressureByLocation.set(location, { kills: 0, thefts: 0, score: 0, lastDay: null, events: [] });
            const signal = pressureByLocation.get(location);
            if (isTheft) signal.thefts++;
            else signal.kills++;
            signal.score += Math.max(0, 1 - age / settings.localEchoLookbackDays);
            signal.lastDay = signal.lastDay === null ? day : Math.max(signal.lastDay, day);
            signal.events.push(event);
          }
        }
        for (const event of items) {
          if (!isSkyRewriteEchoEvent(event)) continue;
          const location = event.location || event.subject || 'Location unavailable';
          if (state.skyrewritePressureLocation !== 'all' && location !== state.skyrewritePressureLocation) continue;
          const plugin = eventPluginOwner(event) || 'Plugin owner unavailable';
          if (state.skyrewritePressurePlugin !== 'all' && (state.skyrewritePressurePlugin === '__unavailable__' ? plugin !== 'Plugin owner unavailable' : plugin !== state.skyrewritePressurePlugin)) continue;
          const recordedPressure = Number(String(event.detail || '').match(/pressure\s+([\d.]+)/i)?.[1]);
          if (!Number.isFinite(recordedPressure)) continue;
          const recordedKills = Number(String(event.detail || '').match(/from\s+(\d+)\s+kills/i)?.[1]) || Number(String(event.detail || '').match(/from\s+(\d+)\s+player kills/i)?.[1]) || Number(event.stage) || 0;
          const recordedThefts = Number(String(event.detail || '').match(/and\s+(\d+)\s+thefts/i)?.[1]) || 0;
          const priorSignal = pressureByLocation.get(location);
          if (priorSignal) {
            priorSignal.recordedEcho = event;
            priorSignal.score = Math.max(priorSignal.score, recordedPressure);
            priorSignal.kills = Math.max(priorSignal.kills, recordedKills);
            priorSignal.thefts = Math.max(priorSignal.thefts || 0, recordedThefts);
          } else pressureByLocation.set(location, { kills: recordedKills, thefts: recordedThefts, score: recordedPressure, lastDay: hasSkyRewriteEventDay(event) ? Number(event.day) : latestPressureDay, events: [], recordedEcho: event });
        }
        const threshold = settings.localEchoKillThreshold;
        const pressureEntries = [...pressureByLocation.entries()].map(([location, signal]) => ({
          location, ...signal,
          band: signal.score >= threshold ? 'threshold' : signal.score >= threshold / 2 ? 'rising' : 'stirring'
        }));
        const pressureNeedle = state.skyrewritePressureSearch.trim().toLocaleLowerCase();
        const visiblePressure = pressureEntries.filter((entry) => {
          const matchesBand = state.skyrewritePressureFilter === 'all' || entry.band === state.skyrewritePressureFilter;
          const signalText = entry.events.map((event) => {
            const formID = Number(event.formID || 0) >>> 0;
            const baseFormID = Number(event.baseFormID || 0) >>> 0;
            return `${event.subject || ''} ${event.detail || ''} ${event.formPlugin || eventPluginOwner(event)} ${event.basePlugin || eventPluginOwner(event, 'base')} ${formID.toString(16).toUpperCase().padStart(8, '0')} ${baseFormID.toString(16).toUpperCase().padStart(8, '0')} ${event.day ?? ''}`;
          }).join(' ');
          return matchesBand && `${entry.location} ${signalText} ${entry.recordedEcho?.detail || ''}`.toLocaleLowerCase().includes(pressureNeedle);
        });
        visiblePressure.sort((a, b) => state.skyrewritePressureSort === 'name'
          ? a.location.localeCompare(b.location)
          : state.skyrewritePressureSort === 'recent'
            ? b.lastDay - a.lastDay || b.score - a.score
            : b.score - a.score || b.kills - a.kills || a.location.localeCompare(b.location));
        const pressureCount = byId('skyrewrite-pressure-count');
        const pressureFilterLabel = state.skyrewritePressurePlugin === 'all' ? '' : ` · ${state.skyrewritePressurePlugin === '__unavailable__' ? 'plugin owner unavailable' : state.skyrewritePressurePlugin}`;
        if (pressureCount) pressureCount.textContent = `${pressureEntries.length} locations · ${pressureEntries.reduce((total, entry) => total + entry.kills, 0)} recent kills · ${pressureEntries.reduce((total, entry) => total + (entry.thefts || 0), 0)} recent thefts${pressureFilterLabel}`;
        const resultCount = byId('skyrewrite-pressure-result-count');
        if (resultCount) resultCount.textContent = `${visiblePressure.length} of ${pressureEntries.length} locations`;
        document.querySelectorAll('[data-skyrewrite-pressure-filter]').forEach((button) => {
          const active = button.dataset.skyrewritePressureFilter === state.skyrewritePressureFilter;
          button.classList.toggle('active', active);
          button.setAttribute('aria-pressed', String(active));
        });
        renderSkyRewritePicker('pressure-sort', getSkyRewritePickerOptions('pressure-sort'), state.skyrewritePressureSort);
        renderSkyRewritePicker('pressure-location', getSkyRewritePickerOptions('pressure-location'), state.skyrewritePressureLocation);
        renderSkyRewritePicker('pressure-plugin', getSkyRewritePickerOptions('pressure-plugin'), state.skyrewritePressurePlugin);
        pressureHost.replaceChildren();
        if (!visiblePressure.length) {
          const empty = document.createElement('div'); empty.className = 'empty';
          empty.textContent = pressureEntries.length ? 'No locations match this pressure search and filter.' : 'No recent player-caused kills or recorded thefts with usable in-game days are in the configured lookback window.';
          pressureHost.appendChild(empty);
        } else {
          for (const entry of visiblePressure) {
            const row = document.createElement('article'); row.className = 'skyrewrite-pressure-row';
            row.dataset.skyrewritePressureBand = entry.band;
            const heading = document.createElement('div'); heading.className = 'skyrewrite-memory-heading';
            const name = document.createElement('strong'); name.textContent = entry.location;
            const label = document.createElement('span'); label.className = `skyrewrite-pressure-band ${entry.band}`;
            label.textContent = entry.recordedEcho ? 'Echo recorded' : entry.band === 'threshold' ? 'Threshold met' : entry.band === 'rising' ? 'Rising' : 'Stirring';
            heading.append(name, label);
            const meter = document.createElement('div'); meter.className = 'skyrewrite-memory-track';
            const fill = document.createElement('span'); fill.style.width = `${Math.min(100, entry.score / threshold * 100)}%`; meter.appendChild(fill);
            const responseEstimate = settings.localEchoTemporaryBounty && entry.score >= threshold
              ? (() => { const base = settings.localEchoTemporaryBountyAmount; const estimate = settings.localEchoTemporaryBountyScaleWithPressure ? Math.max(5, Math.min(100, Math.round(Math.max(5, Math.min(100, base * entry.score / threshold)) / 5) * 5)) : base; return ` · potential hold response: ${estimate} septims${settings.localEchoTemporaryBountyScaleWithPressure ? ' (scaled)' : ''} if the rule fires and the hold is eligible`; })()
              : '';
            const caption = document.createElement('small'); caption.className = 'skyrewrite-pressure-caption'; caption.textContent = entry.recordedEcho && !entry.events.length
              ? `${entry.score.toFixed(2)} / ${threshold} pressure recorded when the rule fired · ${entry.kills} kills · ${entry.thefts || 0} thefts at trigger · ${entry.recordedEcho.detail}`
              : `${entry.score.toFixed(2)} / ${threshold} weighted pressure · ${entry.kills} recorded ${entry.kills === 1 ? 'kill' : 'kills'} · ${entry.thefts || 0} thefts · last ${Math.max(0, latestPressureDay - entry.lastDay).toFixed(1)} in-game days ago${responseEstimate}`;
            const details = document.createElement('details'); details.className = 'skyrewrite-pressure-details';
            details.open = state.skyrewritePressureExpanded.has(entry.location);
            details.addEventListener('toggle', () => { if (details.open) state.skyrewritePressureExpanded.add(entry.location); else state.skyrewritePressureExpanded.delete(entry.location); });
            const summary = document.createElement('summary'); summary.textContent = entry.recordedEcho && !entry.events.length ? `Echo record · ${entry.kills} kills · ${entry.thefts || 0} thefts at trigger` : `Signal sources · ${entry.kills} kills · ${entry.thefts || 0} thefts`; details.appendChild(summary);
            if (entry.recordedEcho && !entry.events.length) {
              const source = document.createElement('div'); source.className = 'skyrewrite-pressure-source';
              const actor = document.createElement('strong'); actor.textContent = 'Recorded Echo outcome';
              const meta = document.createElement('small'); meta.textContent = `${entry.recordedEcho.detail} · ${entry.recordedEcho.day !== null && Number.isFinite(Number(entry.recordedEcho.day)) ? `day ${Number(entry.recordedEcho.day).toFixed(1)}` : 'day unavailable'}`;
              source.append(actor, meta); details.appendChild(source);
            }
            for (const event of [...entry.events].sort((a, b) => Number(b.day) - Number(a.day))) {
              const source = document.createElement('div'); source.className = 'skyrewrite-pressure-source';
              const actor = document.createElement('strong'); actor.textContent = isSkyRewriteTheft(event) ? `Theft · ${event.subject || 'Unknown item'}` : event.subject || 'Unknown actor';
              const id = Number(event.formID || 0).toString(16).toUpperCase().padStart(8, '0');
              const meta = document.createElement('small'); meta.textContent = `Day ${Number(event.day).toFixed(1)} · FormID ${id} · ${event.formPlugin || 'plugin owner unavailable'}`;
              source.append(actor, meta); details.appendChild(source);
            }
            const journal = document.createElement('button'); journal.type = 'button'; journal.className = 'skyrewrite-memory-history'; journal.textContent = 'Open matching history';
            journal.addEventListener('click', () => {
              state.skyrewriteLocationFilter = entry.location;
              state.skyrewritePluginFilter = 'all'; state.skyrewriteTimeRange = String(settings.localEchoLookbackDays);
              state.skyrewriteFilter = 'all'; state.skyrewriteSearch = '';
              const input = byId('skyrewrite-search-input'); if (input) input.value = state.skyrewriteSearch;
              setSkyRewriteTab('history');
              SkyRewriteRenderer.setSkyRewriteHistory();
              byId('skyrewrite-history')?.scrollIntoView({ behavior: 'smooth', block: 'center' });
            });
            const footer = document.createElement('div'); footer.className = 'skyrewrite-pressure-footer'; footer.append(caption, journal);
            row.append(heading, meter, footer, details); pressureHost.appendChild(row);
          }
        }
        } catch (error) {
          console.error('SkyRewrite local pressure render failed', error);
          const detail = String(error?.message || error || 'unknown error').slice(0, 180);
          const pressureCount = byId('skyrewrite-pressure-count');
          if (pressureCount) pressureCount.textContent = 'Local pressure render error';
          const empty = document.createElement('div'); empty.className = 'empty'; empty.textContent = `Local pressure could not render: ${detail}`;
          pressureHost.replaceChildren(empty);
        }
      }
      if (state.skyrewriteTab === 'overview' && locationsHost) {
        const locations = new Map();
        const recentLocations = new Map();
        const dayValues = items.filter((event) => event.day != null && event.day !== '').map((event) => Number(event.day)).filter(Number.isFinite);
        const latestDay = dayValues.length ? Math.max(...dayValues) : null;
        for (const event of items) {
          if (event.kind === 'echo') continue;
          if (!settings.showInventoryEvents && event.kind === 'inventory') continue;
          const name = event.location || 'Location unavailable';
          if (!locations.has(name)) locations.set(name, { deaths: 0, kills: 0, questStages: 0, visits: 0, itemsIn: 0, itemsOut: 0, goldIn: 0, goldOut: 0 });
          const totals = locations.get(name);
          const eventDay = Number(event.day);
          const hasEventDay = latestDay !== null && event.day != null && event.day !== '' && Number.isFinite(eventDay);
          const period = hasEventDay && eventDay <= latestDay && eventDay >= latestDay - 7 ? 'current' : hasEventDay && eventDay < latestDay - 7 && eventDay >= latestDay - 14 ? 'previous' : '';
          if (period) {
            if (!recentLocations.has(name)) recentLocations.set(name, { current: { events: 0, kills: 0, deaths: 0, questStages: 0, visits: 0, transfers: 0, latestDay: -Infinity }, previous: { events: 0, kills: 0, deaths: 0, questStages: 0, visits: 0, transfers: 0, latestDay: -Infinity } });
            const bucket = recentLocations.get(name)[period];
            bucket.events++;
            bucket.latestDay = Math.max(bucket.latestDay, eventDay);
            if (event.kind === 'death') {
              if (event.detail === 'Player killed this actor') bucket.kills++;
              else bucket.deaths++;
            } else if (event.kind === 'quest') bucket.questStages++;
            else if (event.kind === 'location') bucket.visits++;
            else if (event.kind === 'inventory') bucket.transfers++;
          }
          if (event.kind === 'death') {
            if (event.detail === 'Player killed this actor') totals.kills++;
            else totals.deaths++;
          } else if (event.kind === 'quest') totals.questStages++;
          else if (event.kind === 'location') totals.visits++;
          else if (event.kind === 'inventory') {
            const actionIdentity = String(event.identity || '');
            const transferredIn = event.isTheft === true || event.detail?.startsWith('Transferred in') || event.detail?.startsWith('Gained') ||
              /\/(?:picked-up|looted-body|looted-container|received-gift|received-from-npc|purchased)$/.test(actionIdentity);
            if (transferredIn) totals.itemsIn++;
            else totals.itemsOut++;
            if (event.isGold === true || (Number(event.formID) >>> 0) === 0x0000000F || /gold|septim/i.test(String(event.subject || ''))) {
              const quantityMatch = event.detail?.match(/\b(\d+)\s+(?:items?|gold)\b/i) || event.detail?.match(/\b(?:gold|septims?)\s*x\s*(\d+)/i);
              const quantity = Math.max(0, Number(quantityMatch?.[1]) || Number(event.stage) || 0);
              if (transferredIn) totals.goldIn += quantity;
              else totals.goldOut += quantity;
            }
          }
        }
        const recentHost = byId('skyrewrite-recent-activity');
        if (recentHost) {
          recentHost.replaceChildren();
          const recentRows = [...recentLocations.entries()].sort((a, b) => b[1].current.events - a[1].current.events || b[1].previous.events - a[1].previous.events);
          if (!recentRows.length) {
            const empty = document.createElement('div'); empty.className = 'empty';
            empty.textContent = 'No dated events in the latest 14 in-game days.';
            recentHost.appendChild(empty);
          } else {
            for (const [name, windows] of recentRows) {
              const current = windows.current;
              const previous = windows.previous;
              const delta = current.events - previous.events;
              const row = document.createElement('button'); row.type = 'button'; row.className = 'skyrewrite-location-row is-link';
              row.setAttribute('aria-label', `Show history for ${name}`);
              row.addEventListener('click', () => selectSkyRewriteLocation(name));
              const title = document.createElement('strong'); title.textContent = name;
              const stats = document.createElement('span');
              const transfers = settings.showInventoryEvents ? ` · ${current.transfers} inventory transfers` : '';
              const change = delta > 0 ? `+${delta}` : String(delta);
              stats.textContent = `${current.events} events vs ${previous.events} in prior 7 days (${change}) · ${current.kills} kills · ${current.deaths} deaths · ${current.questStages} quest stages · ${current.visits} location changes${transfers}`;
              row.append(title, stats); recentHost.appendChild(row);
            }
          }
        }
        locationsHost.replaceChildren();
        if (!locations.size) {
          const empty = document.createElement('div'); empty.className = 'empty';
          empty.textContent = 'No location activity recorded yet.';
          locationsHost.appendChild(empty);
        } else {
          for (const [name, totals] of [...locations.entries()].sort((a, b) => (b[1].kills + b[1].deaths + b[1].questStages + b[1].visits + b[1].itemsIn + b[1].itemsOut) - (a[1].kills + a[1].deaths + a[1].questStages + a[1].visits + a[1].itemsIn + a[1].itemsOut))) {
            const row = document.createElement('button'); row.type = 'button'; row.className = 'skyrewrite-location-row is-link';
            row.setAttribute('aria-label', `Show history for ${name}`);
            row.addEventListener('click', () => selectSkyRewriteLocation(name));
            const title = document.createElement('strong'); title.textContent = name;
            const netGold = totals.goldIn - totals.goldOut;
            const gold = totals.goldIn || totals.goldOut ? ` · Gold in ${totals.goldIn.toLocaleString()} / out ${totals.goldOut.toLocaleString()} · net inventory change ${netGold >= 0 ? '+' : '−'}${Math.abs(netGold).toLocaleString()}` : '';
            const stats = document.createElement('span'); stats.textContent = `${totals.visits} location changes · ${totals.itemsIn} item transfer-in events · ${totals.itemsOut} item transfer-out events · ${totals.kills} player kills · ${totals.deaths} player deaths · ${totals.questStages} quest stage events${gold}`;
            row.append(title, stats); locationsHost.appendChild(row);
          }
        }
      }
    },

};
const SkyRewriteApp = {
  setSkyRewriteHistory() { SkyRewriteRenderer.setSkyRewriteHistory(); },
  selectSkyRewriteLocation(location) {
    state.skyrewriteLocationFilter = location;
    setSkyRewriteTab('history');
    SkyRewriteRenderer.setSkyRewriteHistory();
    byId('skyrewrite-history')?.scrollIntoView({ behavior: 'smooth', block: 'center' });
  },
  openSkyRewriteRuleHistory(location) {
    state.skyrewriteFilter = 'echo'; state.skyrewriteLocationFilter = location || 'all'; state.skyrewritePluginFilter = 'all';
    state.skyrewriteTimeRange = 'all'; state.skyrewriteSearch = '';
    const search = byId('skyrewrite-search-input'); if (search) search.value = '';
    setSkyRewriteTab('history'); SkyRewriteRenderer.setSkyRewriteHistory();
    byId('skyrewrite-history')?.scrollIntoView({ behavior: 'smooth', block: 'center' });
  }
};
function bindSkyRewriteDashboard() {
  document.querySelectorAll('[data-skyrewrite-tab]').forEach((button) => button.addEventListener('click', () => {
    setSkyRewriteTab(button.dataset.skyrewriteTab); SkyRewriteRenderer.setSkyRewriteHistory();
  }));
  document.querySelectorAll('[data-skyrewrite-hold]').forEach((button) => button.addEventListener('click', () => {
    state.skyrewriteHold = button.dataset.skyrewriteHold || 'eastmarch'; renderSkyRewriteRegionalTrade(state.skyrewriteEvents);
  }));
  byId('skyrewrite-regional-trade')?.addEventListener('click', (event) => {
    const button = event.target.closest('[data-clear-hold-bounty]'); if (!button) return;
    button.disabled = true; button.textContent = 'Clearing both…'; invoke('skyos_skyrewrite_clear_hold_bounty', button.dataset.clearHoldBounty || '');
  });
  document.querySelectorAll('[data-skyrewrite-theft-filter]').forEach((button) => button.addEventListener('click', () => {
    state.skyrewriteTheftFilter = button.dataset.skyrewriteTheftFilter || 'all'; renderSkyRewriteTheftHistory(state.skyrewriteEvents);
  }));
  byId('skyrewrite-theft-search')?.addEventListener('input', (event) => { state.skyrewriteTheftSearch = event.currentTarget.value; renderSkyRewriteTheftHistory(state.skyrewriteEvents); });
  document.querySelectorAll('[data-skyrewrite-filter]').forEach((button) => button.addEventListener('click', () => {
    state.skyrewriteFilter = button.dataset.skyrewriteFilter || 'all'; SkyRewriteRenderer.setSkyRewriteHistory();
  }));
  document.querySelectorAll('.skyrewrite-picker').forEach((picker) => {
    const name = picker.dataset.skyrewritePicker, trigger = picker.querySelector('.skyrewrite-picker-trigger');
    const popover = picker.querySelector('.skyrewrite-picker-popover'), search = picker.querySelector('.skyrewrite-picker-search input');
    trigger.addEventListener('click', () => {
      const opening = popover.hidden;
      document.querySelectorAll('.skyrewrite-picker-popover').forEach((menu) => { menu.hidden = true; menu.parentElement.querySelector('.skyrewrite-picker-trigger')?.setAttribute('aria-expanded', 'false'); });
      popover.hidden = !opening; trigger.setAttribute('aria-expanded', opening ? 'true' : 'false');
      if (opening && search) { search.value = ''; renderSkyRewritePicker(name, getSkyRewritePickerOptions(name), getSkyRewritePickerValue(name)); search.focus(); }
    });
    search?.addEventListener('input', () => renderSkyRewritePicker(name, getSkyRewritePickerOptions(name), getSkyRewritePickerValue(name)));
    picker.querySelector('.skyrewrite-picker-options')?.addEventListener('click', (event) => {
      const option = event.target.closest('.skyrewrite-picker-option'); if (!option) return;
      const value = option.dataset.value;
      if (name === 'location') state.skyrewriteLocationFilter = value;
      else if (name === 'plugin') state.skyrewritePluginFilter = value;
      else if (name === 'actor-grouping') state.skyrewriteActorGrouping = value;
      else if (name === 'actor-sort') state.skyrewriteActorSort = value;
      else if (name === 'actor-location') state.skyrewriteActorLocation = value;
      else if (name === 'actor-plugin') state.skyrewriteActorPlugin = value;
      else if (name === 'pressure-sort') state.skyrewritePressureSort = value;
      else if (name === 'pressure-location') state.skyrewritePressureLocation = value;
      else if (name === 'pressure-plugin') state.skyrewritePressurePlugin = value;
      else state.skyrewriteTimeRange = value;
      popover.hidden = true; trigger.setAttribute('aria-expanded', 'false');
      const label = trigger.querySelector('span'), chosen = getSkyRewritePickerOptions(name).find((entry) => entry.value === value);
      if (label && chosen) label.textContent = chosen.label;
      SkyRewriteRenderer.setSkyRewriteHistory();
    });
    search?.addEventListener('keydown', (event) => { if (event.key === 'Escape') { popover.hidden = true; trigger.setAttribute('aria-expanded', 'false'); trigger.focus(); } });
  });
  document.addEventListener('click', (event) => {
    if (event.target.closest('.skyrewrite-picker')) return;
    document.querySelectorAll('.skyrewrite-picker-popover:not([hidden])').forEach((menu) => { menu.hidden = true; menu.parentElement.querySelector('.skyrewrite-picker-trigger')?.setAttribute('aria-expanded', 'false'); });
  });
  const inputs = [
    ['skyrewrite-custom-days', 'time'], ['skyrewrite-search-input', 'search'], ['skyrewrite-memory-search', 'memory'],
    ['skyrewrite-pressure-search', 'pressure'], ['skyrewrite-actor-search', 'actor']
  ];
  for (const [id, field] of inputs) byId(id)?.addEventListener('input', (event) => {
    if (field === 'time') {
      const value = Number(event.target.value);
      if (Number.isFinite(value) && value > 0) state.skyrewriteTimeRange = String(value);
      else if (!event.target.value && !['all', '7', '30'].includes(String(state.skyrewriteTimeRange))) state.skyrewriteTimeRange = 'all';
    } else if (field === 'search') state.skyrewriteSearch = event.target.value;
    else if (field === 'memory') state.skyrewriteMemorySearch = event.target.value || '';
    else if (field === 'pressure') state.skyrewritePressureSearch = event.target.value || '';
    else if (field === 'actor') state.skyrewriteActorSearch = event.target.value || '';
    SkyRewriteRenderer.setSkyRewriteHistory();
  });
  byId('skyrewrite-custom-days')?.addEventListener('change', (event) => {
    const input = event.currentTarget;
    if (!input.value.trim()) {
      state.skyrewriteTimeRange = 'all';
      SkyRewriteRenderer.setSkyRewriteHistory();
      return;
    }
    const value = Number(input.value);
    if (!Number.isFinite(value)) return;
    const boundedValue = Math.max(0.1, Math.min(36500, value));
    input.value = String(boundedValue);
    state.skyrewriteTimeRange = String(boundedValue);
    SkyRewriteRenderer.setSkyRewriteHistory();
  });
  byId('skyrewrite-bounty-ledger')?.addEventListener('click', (event) => {
    const undo = event.target.closest('[data-undo-bounty]');
    if (undo) {
      if (undo.dataset.confirming === 'true') { undo.dataset.confirming = 'false'; undo.disabled = true; undo.textContent = 'Clearing…'; invoke('skyos_skyrewrite_undo_temporary_bounty', undo.dataset.identity || ''); }
      else { undo.dataset.confirming = 'true'; undo.textContent = 'Confirm clear'; window.setTimeout(() => { if (undo.isConnected && undo.dataset.confirming === 'true') { undo.dataset.confirming = 'false'; undo.textContent = 'Clear bounty'; } }, 5000); }
      return;
    }
    const history = event.target.closest('[data-open-bounty-history]'); if (history) SkyRewriteApp.openSkyRewriteRuleHistory(history.dataset.location || '');
  });
  byId('skyrewrite-game-bounty-ledger')?.addEventListener('click', (event) => {
    const button = event.target.closest('[data-clear-game-bounty]'); if (!button) return;
    if (button.dataset.confirming === 'true') { button.dataset.confirming = 'false'; button.disabled = true; button.textContent = 'Clearing…'; invoke('skyos_skyrewrite_clear_hold_bounty', button.dataset.clearGameBounty || ''); }
    else { button.dataset.confirming = 'true'; button.textContent = 'Confirm clear'; window.setTimeout(() => { if (button.isConnected && button.dataset.confirming === 'true') { button.dataset.confirming = 'false'; button.textContent = 'Clear bounty'; } }, 5000); }
  });
  for (const [selector, key, dataKey] of [
    ['[data-skyrewrite-memory-filter]', 'skyrewriteMemoryFilter', 'skyrewriteMemoryFilter'],
    ['[data-skyrewrite-pressure-filter]', 'skyrewritePressureFilter', 'skyrewritePressureFilter'],
    ['[data-skyrewrite-actor-filter]', 'skyrewriteActorFilter', 'skyrewriteActorFilter']
  ]) {
    document.querySelectorAll(selector).forEach((button) => button.addEventListener('click', () => {
      state[key] = button.dataset[dataKey] || 'all';
      SkyRewriteRenderer.setSkyRewriteHistory();
    }));
  }
  const clear = byId('skyrewrite-clear-history'); let clearTimer = 0;
  clear?.addEventListener('click', () => {
    if (clear.dataset.confirming === 'true') { window.clearTimeout(clearTimer); clear.dataset.confirming = 'false'; clear.textContent = 'Clear journal'; invoke('skyos_skyrewrite_clear_history'); return; }
    clear.dataset.confirming = 'true'; clear.textContent = 'Confirm clear'; clearTimer = window.setTimeout(() => { clear.dataset.confirming = 'false'; clear.textContent = 'Clear journal'; }, 5000);
  });
}
window.addEventListener('message', (event) => {
  if (event.source !== window.parent || event.data?.channel !== 'skyrewrite-dashboard') return;
  if (event.data.type === 'host-ready') {
    applySkyRewriteSettings();
    window.parent.postMessage({ channel: 'skyrewrite-dashboard', type: 'legacy-settings', payload: state.settings }, '*');
    window.parent.postMessage({ channel: 'skyrewrite-dashboard', type: 'ready' }, '*');
  } else if (event.data.type === 'settings' && event.data.payload && typeof event.data.payload === 'object') {
    state.settings = { ...defaultSkyRewriteSettings, ...state.settings, ...event.data.payload };
    saveSkyRewriteSettings();
  } else if (event.data.type === 'request-settings') {
    window.parent.postMessage({ channel: 'skyrewrite-dashboard', type: 'legacy-settings', payload: state.settings }, '*');
  } else if (event.data.type === 'history') {
    const payload = event.data.payload || {};
    if (Array.isArray(payload.gameBountiesOnly)) {
      state.skyrewriteGameBounties = payload.gameBountiesOnly;
      if (state.activePage === 'skyrewrite' && state.skyrewriteTab === 'bounties') SkyRewriteRenderer.setSkyRewriteHistory();
    } else {
      if (Array.isArray(payload)) state.skyrewriteHistoryLoading = false;
      SkyRewriteRenderer.setSkyRewriteHistory(payload);
      if (Array.isArray(payload.events)) {
        state.skyrewriteHistoryLoading = false;
        if (state.skyrewriteHistoryReloadPending) {
          reloadSkyRewriteHistory();
          return;
        }
        if (state.activePage === 'skyrewrite' && state.skyrewriteVisible && state.skyrewriteTab !== 'history' &&
            state.skyrewriteHistoryHasMore && state.skyrewriteHistoryNextOffset < skyRewriteHistoryMaxRows) {
          state.skyrewriteHistoryLoading = true;
          requestSkyRewriteHistoryPage(state.skyrewriteHistoryNextOffset);
        }
      }
    }
  } else if (event.data.type === 'visibility') {
    setSkyRewriteVisibility(event.data.payload);
  } else if (event.data.type === 'history-changed' && state.skyrewriteVisible) {
    if (state.skyrewriteTimer) window.clearTimeout(state.skyrewriteTimer);
    state.skyrewriteTimer = window.setTimeout(() => {
      state.skyrewriteTimer = 0;
      if (!state.skyrewriteVisible) return;
      reloadSkyRewriteHistory();
      if (state.skyrewriteTab === 'bounties') invoke('skyos_skyrewrite_game_bounties');
    }, state.skyrewriteTab === 'history' ? skyrewriteSettings().historyRefreshInterval : 250);
  } else if (event.data.type === 'action-status') {
    const status = byId('skyrewrite-status'); if (status) status.textContent = String(event.data.payload || 'SkyRewrite');
  }
});
bindSkyRewriteDashboard();
setSkyRewriteTab(state.skyrewriteTab);
window.parent.postMessage({ channel: 'skyrewrite-dashboard', type: 'ready' }, '*');
