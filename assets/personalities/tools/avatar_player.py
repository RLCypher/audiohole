"""GUI-side reference playback policy. No audio/controller APIs or inference.

Requests carry presentation intent from the personality layer. Asset preflight
happens during request(), never during frame_at(). A renderer should preload
decoded images alongside these selections before using a UI animation timer.
"""
from __future__ import annotations

from bisect import bisect_right
from dataclasses import dataclass
from pathlib import Path

from avatar_assets import AssetError, Resolver, Selection, checked_frames, load_pack


DEFAULT_BEHAVIOR = {'mode':'persistent','category':'ambient','priority':0,
                    'interruptible':True,'cooldown_ms':0,'static_hold_ms':1500,
                    'return_to':'persistent','reduced_motion':'static'}


@dataclass(frozen=True)
class RequestResult:
    accepted: bool
    reason: str


@dataclass(frozen=True)
class PresentedFrame:
    personality: str
    style: str
    state: str
    source: str
    path: Path | None
    frame_index: int
    persistent_state: str
    diagnostics: tuple[str, ...] = ()


class AvatarPlayer:
    def __init__(self, approved_only=True):
        self.approved_only = approved_only
        self.resolver = Resolver(approved_only)
        self.identity = None
        self.persistent_state = 'idle'
        self.active_state = 'idle'
        self.current = None
        self.behavior = dict(DEFAULT_BEHAVIOR)
        self.base = None
        self.started = 0
        self.deadline = None
        self.cumulative = ()
        self.last_started = {}
        self.last_now = None
        self.reduced_motion = False

    def _time(self, now_ms):
        if not isinstance(now_ms, int) or isinstance(now_ms, bool) or now_ms < 0:
            raise ValueError('time must be a nonnegative monotonic integer in milliseconds')
        if self.last_now is not None and now_ms < self.last_now:
            raise ValueError('presentation time moved backwards')
        self.last_now = now_ms

    def _choose(self, manifest, personality, style, state, data, reduced):
        entry = data.get('styles', {}).get(style, {}).get('states', {}).get(state, {})
        behavior = {**DEFAULT_BEHAVIOR, **entry.get('behavior', {})}
        if not reduced:
            return self.resolver.resolve(manifest,personality,style,state), behavior
        for target in [state, 'idle']:
            candidate = data.get('styles', {}).get(style, {}).get('states', {}).get(target, {}).get('static')
            if candidate is None:
                continue
            try:
                frames = checked_frames(manifest.parent,candidate,self.approved_only)
                return Selection(personality,style,target,'reduced_motion_static',(frames[0][0],)), behavior
            except AssetError:
                pass
        selection = self.resolver.resolve(manifest,personality,style,state)
        # A pack containing only animation can still offer an owned frozen frame.
        return Selection(personality,style,selection.state,'reduced_motion_freeze',
                         selection.paths[:1],diagnostics=selection.diagnostics), behavior

    def _start(self, state, selection, behavior, now):
        self.active_state, self.current, self.behavior = state, selection, behavior
        self.started = now
        elapsed, cumulative = 0, []
        for duration in selection.durations_ms:
            elapsed += duration
            cumulative.append(elapsed)
        self.cumulative = tuple(cumulative)
        self.deadline = (now+(elapsed or behavior['static_hold_ms'])) if behavior['mode']=='finite' else None

    def request(self, manifest, personality, style, state, now_ms, reduced_motion=False):
        self._time(now_ms)
        manifest = Path(manifest)
        identity = (manifest.resolve(),personality,style)
        if identity != self.identity:
            self.identity = identity
            self.resolver = Resolver(self.approved_only)
            self.current, self.base = None, None
            self.last_started.clear()
            self.persistent_state = 'idle'
        self.frame_at(now_ms)
        try:
            data = load_pack(manifest,personality)
            if style not in data['styles'] or not data['styles'][style]['enabled']:
                raise AssetError('selected style is disabled or unavailable')
        except AssetError as error:
            self.current = Selection(personality,style,state,'hide',diagnostics=(str(error),))
            self.active_state = state
            self.behavior = dict(DEFAULT_BEHAVIOR)
            self.cumulative = ()
            self.base = None
            self.deadline = None
            return RequestResult(False,'invalid_pack_or_style')
        selection, behavior = self._choose(manifest,personality,style,state,data,reduced_motion)
        changed_motion = self.reduced_motion != reduced_motion
        self.reduced_motion = reduced_motion
        if self.base is None or changed_motion:
            base_choice, base_behavior = self._choose(manifest,personality,style,self.persistent_state,data,reduced_motion)
            self.base = (self.persistent_state,base_choice,base_behavior)
        same_media=bool(self.current and self.current.paths and self.current.paths==selection.paths
                        and self.current.durations_ms==selection.durations_ms and self.current.source==selection.source)
        if same_media and self.active_state == state and not changed_motion:
            return RequestResult(False,'already_active')
        if not changed_motion and now_ms-self.last_started.get(state,-3600001) < behavior['cooldown_ms']:
            return RequestResult(False,'cooldown')
        if behavior['mode'] == 'persistent':
            self.persistent_state = state
            self.base = (state,selection,behavior)
            if self.current and self.behavior['mode']=='finite' and behavior['priority'] <= self.behavior['priority']:
                return RequestResult(True,'persistent_context_updated')
        if self.current and self.behavior['mode']=='finite' and not changed_motion:
            old_priority, new_priority = self.behavior['priority'], behavior['priority']
            if new_priority < old_priority or (not self.behavior['interruptible'] and new_priority <= old_priority):
                return RequestResult(False,'priority_or_interrupt_policy')
        self._start(state,selection,behavior,now_ms)
        self.last_started[state] = now_ms
        return RequestResult(True,'started')

    def frame_at(self, now_ms):
        self._time(now_ms)
        if self.current is None:
            return None
        if self.deadline is not None and now_ms >= self.deadline and self.base is not None:
            state, selection, behavior = self.base
            self._start(state,selection,behavior,self.deadline)
        selection = self.current
        index = 0
        if self.cumulative and selection.paths:
            elapsed = max(0,now_ms-self.started)
            if selection.loop:
                elapsed %= self.cumulative[-1]
            else:
                elapsed = min(elapsed,self.cumulative[-1]-1)
            index = min(bisect_right(self.cumulative,elapsed),len(selection.paths)-1)
        return PresentedFrame(selection.personality,selection.style,self.active_state,selection.source,
                              selection.paths[index] if selection.paths else None,index,
                              self.persistent_state,selection.diagnostics)
