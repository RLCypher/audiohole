# UwU body performance revision

Norm and Vanilla are approved. Their operating pixels and all accepted IDLE
assets remain unchanged. UwU's facial designs are the accepted direction; the
new full-body drawings below remain candidates for human review.

| States | Body acting |
|---|---|
| Error | Draw inward, clutch headphones, buckle knees, incredulous open hand, recover |
| Angry | Anticipation, tightly crossed arms, raised boot and stomp, offended recovery |
| Surprised | Shoulder rise, arms fling outward, recoiling knees, hand to mouth, settle |
| Celebrating | Crouch, raised victory fists, fist pump and lifted knee, open-handed recovery |
| Panicking | Hands near cheeks, alternate high/side arms and lifted knees, settle |
| Happy / thinking | Welcoming arms and knee flex / hand to chin and cheek, hip shift |
| Working / talking | Inspect connector and cable / conversational palm and closing mouth |
| Confused / concerned | Two articulated shrugs / clasped hands and protective folded arms |
| Sleeping / smug | Head rests on folded arms on case / hip stance and finger flourish |
| Embarrassed / laughing | Mug at chin and hand to cheek / mouth-covering and belly-clutching laugh |
| Suspicious / bored | Inspect cable and point / slouched cheek-on-hand resting stance |
| Mischievous / deadpan | Teasing point and beckon / limp-arm static freeze |

The five theatrical states each use four model-drawn body poses. Other animated
states use two. Deadpan uses one still drawing; its second source drawing remains
in the intermediate set. In total the retained source set contains 48 pose keys.
The 18 sequences still expose 12–22 operating frames over 1.2–4 seconds, including
repeated drawings as deliberate holds. They are limited anime key animation,
not fully drawn in-betweens. The case stripe provides scene registration; no
UwU sequence translates or rotates a finished resting cutout to simulate acting.

[Looping three-state demo](uwu-body-revision-loop.gif) is a review-only montage.
[All body keys](uwu-body-keyposes.png) shows every retained drawing. Individual
playable pages and frame sheets are in [UwU's catalog](uwu/index.html). Native
512×768 and compact 128×192 previews are supplied. Finite operating reactions
start and finish at exact approved rest; persistent endpoints match exactly.

Reference-guided generation cannot guarantee identical clothing folds, accessory
placement, case lettering, hand anatomy or head size between drawings. Facial
*designs* are retained through reference guidance; the new faces are newly drawn,
not copied pixels. Inspect snaps to/from accepted IDLE, independent limb motion,
body proportions, case wobble, and the held-pose rhythm. Further in-between
artwork is needed for smooth motion. A first celebration layout and one sheet
with torso/neckline drift were rejected and retained only for provenance; they
were not used for final operating artwork.

`provenance/uwu-body-prompts.json`, `uwu-body-settings.json`, individual
`uwu-*-body-revision.json` records and the pinned packager document the revision.
Original face-only candidates remain recoverable at commit `ac57857`; their
provenance now identifies the replacement records. Original canonical files
and accepted IDLE files have not been repainted.
