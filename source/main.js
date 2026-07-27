// {"name": "Star Fox", "author": "Daniel Santos", "version": "07112026", "icon": "render_icon.png", "file": "starfox.js"}

const font = new Font("default");
font.scale = 0.6f;
font.outline = 1.0f;
font.outline_color = Color.new(0, 0, 0);

Screen.setFrameCounter(true);
Screen.setVSync(true);

const canvas = Screen.getMode();
canvas.zbuffering = true;
canvas.psm = Screen.CT32;
canvas.psmz = Screen.Z24;
Screen.setMode(canvas);
Screen.initBuffers();

Render.init();
Render.setView(72.0, 0.5, 900.0);

const CW = canvas.width;
const CH = canvas.height;

// ---------------------------------------------------------------------------
// Palette / tuning
// ---------------------------------------------------------------------------
const SPACE = Color.new(6, 8, 26);
const C_WHITE = Color.new(230, 240, 255);
const C_CYAN = Color.new(120, 220, 255);
const C_YELLOW = Color.new(255, 214, 90);
const C_RED = Color.new(255, 90, 90);
const C_GREEN = Color.new(120, 255, 150);
const C_DIM = Color.new(120, 140, 180);

const STAR_COUNT = 56;
const STAR_SPREAD_X = 90.0f;
const STAR_SPREAD_Y = 55.0f;
const STAR_Z_NEAR = 150.0f;
const STAR_Z_FAR = 340.0f;
const MAX_PARTICLES = 72;
const MAX_PLANETS = 4;
const BOUNDS_X = 6.0f;
const BOUNDS_Y_LOW = -2.4f;
const BOUNDS_Y_HIGH = 3.4f;
const SHIP_ACCEL = 0.16f;
const BASE_SPEED = 0.85f;
const FIRE_COOLDOWN = 8;
const MAX_BULLETS = 40;
const MAX_ENEMIES = 10;
const VOXEL_GRID = 5;
const VOXEL_MAX = 16;
const VOXEL_CELL = 0.30f;
const VOXEL_DESTROY_AT = 0;

const STATE_MENU = 0;
const STATE_PLAY = 1;
const STATE_OVER = 2;
const STATE_HANGAR = 3;

const SAVE_PATH = System.boot_path + "/starfox_save.json";

// ---------------------------------------------------------------------------
// Roguelike catalogs
// ---------------------------------------------------------------------------
const SHIPS = {
    scout: {
        id: "scout", name: "Scout", price: 0,
        base: { cooldown: 8, bullets: 1, spread: 0.0, damage: 1, lives: 3, speed: 1.0 },
        body: [
            { cx: 0.0, cy: 0.0, cz: 0.0, sx: 0.5, sy: 0.28, sz: 1.6, accent: 0 },
            { cx: 0.0, cy: 0.14, cz: 0.32, sx: 0.24, sy: 0.14, sz: 0.5, accent: 1 },
            { cx: -0.66, cy: 0.14, cz: -0.35, sx: 0.07, sy: 0.4, sz: 0.42, accent: 0 },
            { cx: 0.66, cy: 0.14, cz: -0.35, sx: 0.07, sy: 0.4, sz: 0.42, accent: 0 },
            { cx: 0.0, cy: 0.28, cz: -0.72, sx: 0.07, sy: 0.5, sz: 0.4, accent: 0 },
        ],
    },
    raptor: {
        id: "raptor", name: "Raptor", price: 120,
        base: { cooldown: 6, bullets: 1, spread: 0.0, damage: 1, lives: 2, speed: 1.15 },
        body: [
            { cx: 0.0, cy: 0.0, cz: 0.0, sx: 0.42, sy: 0.22, sz: 1.5, accent: 0 },
            { cx: 0.0, cy: 0.12, cz: 0.28, sx: 0.2, sy: 0.12, sz: 0.45, accent: 1 },
            { cx: 0.0, cy: 0.22, cz: -0.65, sx: 0.06, sy: 0.45, sz: 0.35, accent: 0 },
        ],
    },
    titan: {
        id: "titan", name: "Titan", price: 200,
        base: { cooldown: 10, bullets: 1, spread: 0.0, damage: 2, lives: 5, speed: 0.82 },
        body: [
            { cx: 0.0, cy: 0.0, cz: 0.0, sx: 0.62, sy: 0.34, sz: 1.75, accent: 0 },
            { cx: 0.0, cy: 0.16, cz: 0.35, sx: 0.28, sy: 0.16, sz: 0.55, accent: 1 },
            { cx: -0.72, cy: 0.12, cz: -0.2, sx: 0.08, sy: 0.35, sz: 0.5, accent: 0 },
            { cx: 0.72, cy: 0.12, cz: -0.2, sx: 0.08, sy: 0.35, sz: 0.5, accent: 0 },
        ],
    },
};

const PARTS = {
    wing: [
        { id: "std", name: "Std Wings", price: 0, mod: {}, mesh: [
            { cx: -1.0, cy: 0.0, cz: -0.1, sx: 1.4, sy: 0.06, sz: 0.6, accent: 0 },
            { cx: 1.0, cy: 0.0, cz: -0.1, sx: 1.4, sy: 0.06, sz: 0.6, accent: 0 },
            { cx: -1.75, cy: 0.0, cz: -0.15, sx: 0.3, sy: 0.1, sz: 0.45, accent: 1 },
            { cx: 1.75, cy: 0.0, cz: -0.15, sx: 0.3, sy: 0.1, sz: 0.45, accent: 1 },
        ]},
        { id: "wide", name: "Wide Wings", price: 80, mod: { speed: -0.05, lives: 1 }, mesh: [
            { cx: -1.2, cy: 0.0, cz: -0.05, sx: 1.75, sy: 0.05, sz: 0.55, accent: 0 },
            { cx: 1.2, cy: 0.0, cz: -0.05, sx: 1.75, sy: 0.05, sz: 0.55, accent: 0 },
        ]},
        { id: "dart", name: "Dart Wings", price: 65, mod: { speed: 0.12, cooldown: -1 }, mesh: [
            { cx: -0.85, cy: 0.0, cz: -0.2, sx: 1.0, sy: 0.04, sz: 0.7, accent: 0 },
            { cx: 0.85, cy: 0.0, cz: -0.2, sx: 1.0, sy: 0.04, sz: 0.7, accent: 0 },
        ]},
    ],
    engine: [
        { id: "std", name: "Std Engine", price: 0, mod: {}, mesh: [
            { cx: 0.0, cy: 0.0, cz: -0.86, sx: 0.34, sy: 0.22, sz: 0.2, accent: 0 },
        ]},
        { id: "turbo", name: "Turbo", price: 90, mod: { speed: 0.18, cooldown: -2 }, mesh: [
            { cx: 0.0, cy: 0.0, cz: -0.95, sx: 0.42, sy: 0.28, sz: 0.28, accent: 1 },
        ]},
        { id: "heavy", name: "Heavy", price: 70, mod: { lives: 1, speed: -0.08 }, mesh: [
            { cx: 0.0, cy: 0.0, cz: -0.88, sx: 0.48, sy: 0.3, sz: 0.32, accent: 0 },
        ]},
    ],
    cannon: [
        { id: "std", name: "Pulse Laser", price: 0, mod: {}, mesh: []},
        { id: "twin", name: "Twin Mount", price: 100, mod: { bullets: 1, spread: 0.35 }, mesh: [
            { cx: -0.35, cy: -0.05, cz: 0.5, sx: 0.12, sy: 0.12, sz: 0.35, accent: 1 },
            { cx: 0.35, cy: -0.05, cz: 0.5, sx: 0.12, sy: 0.12, sz: 0.35, accent: 1 },
        ]},
        { id: "rail", name: "Rail Gun", price: 110, mod: { damage: 1, cooldown: 2 }, mesh: [
            { cx: 0.0, cy: -0.08, cz: 0.65, sx: 0.18, sy: 0.14, sz: 0.55, accent: 1 },
        ]},
    ],
    nose: [
        { id: "std", name: "Std Nose", price: 0, mod: {}, mesh: [
            { cx: 0.0, cy: 0.0, cz: 1.0, sx: 0.3, sy: 0.18, sz: 0.55, accent: 0 },
            { cx: 0.0, cy: -0.02, cz: 1.35, sx: 0.14, sy: 0.1, sz: 0.3, accent: 1 },
        ]},
        { id: "probe", name: "Probe Nose", price: 55, mod: { damage: 1 }, mesh: [
            { cx: 0.0, cy: 0.0, cz: 1.15, sx: 0.22, sy: 0.14, sz: 0.75, accent: 0 },
            { cx: 0.0, cy: 0.0, cz: 1.55, sx: 0.08, sy: 0.08, sz: 0.35, accent: 1 },
        ]},
        { id: "blunt", name: "Blunt Nose", price: 45, mod: { lives: 1 }, mesh: [
            { cx: 0.0, cy: 0.0, cz: 0.95, sx: 0.38, sy: 0.22, sz: 0.5, accent: 0 },
        ]},
    ],
};

const COLORS = [
    { id: "arwing", name: "Arwing", price: 0, primary: [0.72, 0.76, 0.86], accent: [0.35, 0.78, 0.98] },
    { id: "crimson", name: "Crimson", price: 40, primary: [0.82, 0.22, 0.18], accent: [1.0, 0.45, 0.15] },
    { id: "jade", name: "Jade", price: 40, primary: [0.22, 0.72, 0.48], accent: [0.55, 1.0, 0.72] },
    { id: "void", name: "Void", price: 55, primary: [0.28, 0.26, 0.38], accent: [0.72, 0.35, 0.95] },
    { id: "gold", name: "Gold", price: 80, primary: [0.85, 0.72, 0.22], accent: [1.0, 0.92, 0.45] },
];

const UPGRADE_DEFS = {
    firerate: { name: "Fire Rate", max: 5, baseCost: 45,
        apply: function (s, lv) { s.cooldown = Math.max(3, s.cooldown - lv); } },
    multishot: { name: "Multi-Shot", max: 3, baseCost: 70,
        apply: function (s, lv) {
            s.bullets = s.bullets + lv;
            if (lv > 0 && s.bullets > 1) s.spread = Math.max(s.spread, 0.22 + lv * 0.14);
        } },
    damage: { name: "Damage", max: 4, baseCost: 55,
        apply: function (s, lv) { s.damage = s.damage + lv; } },
    hull: { name: "Hull", max: 3, baseCost: 60,
        apply: function (s, lv) { s.lives = s.lives + lv; } },
};

const SHIP_IDS = ["scout", "raptor", "titan"];
const PART_SLOTS = ["wing", "engine", "cannon", "nose"];
const UPGRADE_KEYS = ["firerate", "multishot", "damage", "hull"];
const HANGAR_TABS = ["SHIPS", "PARTS", "COLORS", "UPGRADES", "LAUNCH"];

function defaultSave() {
    return {
        credits: 0,
        best: 0,
        ownedShips: ["scout"],
        ownedParts: { wing: ["std"], engine: ["std"], cannon: ["std"], nose: ["std"] },
        ownedColors: ["arwing"],
        upgrades: { firerate: 0, multishot: 0, damage: 0, hull: 0 },
        loadout: { ship: "scout", wing: "std", engine: "std", cannon: "std", nose: "std", color: "arwing" },
    };
}

function mergeSave(raw) {
    const d = defaultSave();
    if (!raw) return d;
    if (raw.credits !== undefined) d.credits = raw.credits;
    if (raw.best !== undefined) d.best = raw.best;
    if (raw.ownedShips) d.ownedShips = raw.ownedShips;
    if (raw.ownedParts) {
        for (let i = 0; i < PART_SLOTS.length; i++) {
            const k = PART_SLOTS[i];
            if (raw.ownedParts[k]) d.ownedParts[k] = raw.ownedParts[k];
        }
    }
    if (raw.ownedColors) d.ownedColors = raw.ownedColors;
    if (raw.upgrades) {
        d.upgrades.firerate = raw.upgrades.firerate || 0;
        d.upgrades.multishot = raw.upgrades.multishot || 0;
        d.upgrades.damage = raw.upgrades.damage || 0;
        d.upgrades.hull = raw.upgrades.hull || 0;
    }
    if (raw.loadout) d.loadout = raw.loadout;
    return d;
}

function loadSave() {
    try {
        const f = std.open(SAVE_PATH, "r");
        if (!f) return defaultSave();
        const line = f.getline();
        f.close();
        if (!line) return defaultSave();
        return mergeSave(JSON.parse(line));
    } catch (e) {
        return defaultSave();
    }
}

function saveGame() {
    try {
        const f = std.open(SAVE_PATH, "w");
        if (!f) return;
        f.puts(JSON.stringify(save));
        f.close();
    } catch (e) {}
}

function findPart(slot, id) {
    const list = PARTS[slot];
    for (let i = 0; i < list.length; i++) {
        if (list[i].id === id) return list[i];
    }
    return list[0];
}

function findColor(id) {
    for (let i = 0; i < COLORS.length; i++) {
        if (COLORS[i].id === id) return COLORS[i];
    }
    return COLORS[0];
}

function ownsShip(id) {
    for (let i = 0; i < save.ownedShips.length; i++) {
        if (save.ownedShips[i] === id) return true;
    }
    return false;
}

function ownsPart(slot, id) {
    const list = save.ownedParts[slot];
    for (let i = 0; i < list.length; i++) {
        if (list[i] === id) return true;
    }
    return false;
}

function ownsColor(id) {
    for (let i = 0; i < save.ownedColors.length; i++) {
        if (save.ownedColors[i] === id) return true;
    }
    return false;
}

function upgradeCost(key, level) {
    return UPGRADE_DEFS[key].baseCost + level * 35;
}

function computeStats(sv) {
    const hull = SHIPS[sv.loadout.ship] || SHIPS.scout;
    const s = {
        cooldown: hull.base.cooldown,
        bullets: hull.base.bullets,
        spread: hull.base.spread,
        damage: hull.base.damage,
        lives: hull.base.lives,
        speed: hull.base.speed,
    };

    for (let i = 0; i < PART_SLOTS.length; i++) {
        const slot = PART_SLOTS[i];
        const p = findPart(slot, sv.loadout[slot]);
        if (!p.mod) continue;
        if (p.mod.cooldown) s.cooldown += p.mod.cooldown;
        if (p.mod.bullets) s.bullets += p.mod.bullets;
        if (p.mod.spread) s.spread += p.mod.spread;
        if (p.mod.damage) s.damage += p.mod.damage;
        if (p.mod.lives) s.lives += p.mod.lives;
        if (p.mod.speed) s.speed += p.mod.speed;
    }

    UPGRADE_DEFS.firerate.apply(s, sv.upgrades.firerate);
    UPGRADE_DEFS.multishot.apply(s, sv.upgrades.multishot);
    UPGRADE_DEFS.damage.apply(s, sv.upgrades.damage);
    UPGRADE_DEFS.hull.apply(s, sv.upgrades.hull);

    s.cooldown = Math.max(3, s.cooldown);
    s.bullets = Math.max(1, s.bullets);
    s.damage = Math.max(1, s.damage);
    s.lives = Math.max(1, s.lives);
    s.speed = Math.max(0.5, s.speed);
    return s;
}

function addBoxes(b, boxes, col) {
    for (let i = 0; i < boxes.length; i++) {
        const bx = boxes[i];
        const c = bx.accent ? col.accent : col.primary;
        b.box(bx.cx, bx.cy, bx.cz, bx.sx, bx.sy, bx.sz, c[0], c[1], c[2], 1.0f);
    }
}

function buildShipMesh(loadout) {
    const hull = SHIPS[loadout.ship] || SHIPS.scout;
    const col = findColor(loadout.color);
    const b = new MeshBuilder();
    addBoxes(b, hull.body, col);
    for (let i = 0; i < PART_SLOTS.length; i++) {
        const slot = PART_SLOTS[i];
        const p = findPart(slot, loadout[slot]);
        if (p.mesh && p.mesh.length > 0) addBoxes(b, p.mesh, col);
    }
    return buildMeshData(b);
}

// Cache every built ship RenderObject by loadout signature. shareBuffers meshes must NOT be
// orphaned: if the GC finalizes an unreferenced RenderData mid-run, its shared vertex buffers
// are freed while still in flight and the renderer crashes. Keeping them all alive avoids that.
const shipCache = {};

function loadoutKey(l) {
    return l.ship + "|" + l.wing + "|" + l.engine + "|" + l.cannon + "|" + l.nose + "|" + l.color;
}

function rebuildShip() {
    const key = loadoutKey(save.loadout);
    let obj = shipCache[key];
    if (!obj) {
        obj = new RenderObject(buildShipMesh(save.loadout));
        shipCache[key] = obj;
    }
    ship = obj;
}

function clamp(v, lo, hi) { return v < lo ? lo : (v > hi ? hi : v); }
function lerp(a, b, t) { return a + (b - a) * t; }

function normalize3(x, y, z) {
    const len = Math.sqrt(x * x + y * y + z * z);
    if (len < 0.0001) return { x: 0.0, y: 0.0, z: 1.0 };
    return { x: x / len, y: y / len, z: z / len };
}

// Rotate a +Z-facing hemisphere so its pole looks at the camera (reads as a flat disc).
function billboardToCamera(px, py, pz, cx, cy, cz) {
    const dx = cx - px;
    const dy = cy - py;
    const dz = cz - pz;
    const horiz = Math.sqrt(dx * dx + dz * dz);
    if (horiz < 0.0001 && Math.abs(dy) < 0.0001) {
        return { x: 0.0, y: 0.0, z: 0.0 };
    }
    const yaw = Math.atan2(dx, dz);
    const pitch = Math.atan2(-dy, horiz);
    return { x: pitch, y: yaw, z: 0.0 };
}

function rotateX(v, a) {
    const c = Math.cos(a), s = Math.sin(a);
    return { x: v.x, y: v.y * c - v.z * s, z: v.y * s + v.z * c };
}

function rotateY(v, a) {
    const c = Math.cos(a), s = Math.sin(a);
    return { x: v.x * c + v.z * s, y: v.y, z: -v.x * s + v.z * c };
}

function rotateZ(v, a) {
    const c = Math.cos(a), s = Math.sin(a);
    return { x: v.x * c - v.y * s, y: v.x * s + v.y * c, z: v.z };
}

function worldToLocal(wx, wy, wz, ex, ey, ez, sc, rx, ry, rz) {
    let v = {
        x: (wx - ex) / sc,
        y: (wy - ey) / sc,
        z: (wz - ez) / sc,
    };
    v = rotateZ(v, -rz);
    v = rotateY(v, -ry);
    v = rotateX(v, -rx);
    return v;
}

function localToWorld(lx, ly, lz, ex, ey, ez, sc, rx, ry, rz) {
    let v = rotateX({ x: lx, y: ly, z: lz }, rx);
    v = rotateY(v, ry);
    v = rotateZ(v, rz);
    return { x: ex + v.x * sc, y: ey + v.y * sc, z: ez + v.z * sc };
}

function voxelHash(ix, iy, iz, seed) {
    return ((ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ seed) & 255;
}

function voxelColor(ix, iy, iz, seed) {
    const t = voxelHash(ix, iy, iz, seed) / 255.0;
    return {
        r: lerp(0.40f, 0.64f, t),
        g: lerp(0.36f, 0.56f, t),
        b: lerp(0.32f, 0.50f, t),
    };
}

const LIGHT = normalize3(0.35, 0.9, -0.28);
function faceLight(nx, ny, nz) {
    const d = nx * LIGHT.x + ny * LIGHT.y + nz * LIGHT.z;
    return 0.4 + 0.6 * (d > 0 ? d : 0);
}

// ---------------------------------------------------------------------------
// Mesh builder with baked directional shading
// ---------------------------------------------------------------------------
function MeshBuilder() {
    this.positions = [];
    this.normals = [];
    this.texcoords = [];
    this.colors = [];
    this.lit = true;
}

MeshBuilder.prototype.vertex = function (x, y, z, nx, ny, nz, r, g, b, a) {
    this.positions.push(x, y, z, 1.0f);
    this.normals.push(nx, ny, nz, 0.0f);
    this.texcoords.push(0.0f, 0.0f, 1.0f, 1.0f);
    this.colors.push(r, g, b, a);
};

MeshBuilder.prototype.tri = function (v0, v1, v2, nx, ny, nz, r, g, b, a) {
    let cr = r, cg = g, cb = b;
    if (this.lit) {
        const l = faceLight(nx, ny, nz);
        cr = clamp(r * l, 0.0, 1.0);
        cg = clamp(g * l, 0.0, 1.0);
        cb = clamp(b * l, 0.0, 1.0);
    }
    this.vertex(v0.x, v0.y, v0.z, nx, ny, nz, cr, cg, cb, a);
    this.vertex(v1.x, v1.y, v1.z, nx, ny, nz, cr, cg, cb, a);
    this.vertex(v2.x, v2.y, v2.z, nx, ny, nz, cr, cg, cb, a);
};

MeshBuilder.prototype.quad = function (v0, v1, v2, v3, nx, ny, nz, r, g, b, a) {
    this.tri(v0, v1, v2, nx, ny, nz, r, g, b, a);
    this.tri(v0, v2, v3, nx, ny, nz, r, g, b, a);
};

MeshBuilder.prototype.box = function (cx, cy, cz, sx, sy, sz, r, g, b, a) {
    const hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
    const A = { x: cx - hx, y: cy - hy, z: cz - hz };
    const B = { x: cx + hx, y: cy - hy, z: cz - hz };
    const C = { x: cx + hx, y: cy + hy, z: cz - hz };
    const D = { x: cx - hx, y: cy + hy, z: cz - hz };
    const E = { x: cx - hx, y: cy - hy, z: cz + hz };
    const F = { x: cx + hx, y: cy - hy, z: cz + hz };
    const G = { x: cx + hx, y: cy + hy, z: cz + hz };
    const H = { x: cx - hx, y: cy + hy, z: cz + hz };

    this.quad(E, F, G, H, 0.0, 0.0, 1.0, r, g, b, a);
    this.quad(B, A, D, C, 0.0, 0.0, -1.0, r, g, b, a);
    this.quad(F, B, C, G, 1.0, 0.0, 0.0, r, g, b, a);
    this.quad(A, E, H, D, -1.0, 0.0, 0.0, r, g, b, a);
    this.quad(H, G, C, D, 0.0, 1.0, 0.0, r, g, b, a);
    this.quad(A, B, F, E, 0.0, -1.0, 0.0, r, g, b, a);
};

MeshBuilder.prototype.dome = function (br, bg, bb, hr, hg, hb, rings, sectors) {
    for (let i = 0; i < rings; i++) {
        const lat0 = (Math.PI * 0.5 * i) / rings;
        const lat1 = (Math.PI * 0.5 * (i + 1)) / rings;
        for (let j = 0; j < sectors; j++) {
            const lon0 = (Math.PI * 2.0 * j) / sectors;
            const lon1 = (Math.PI * 2.0 * (j + 1)) / sectors;

            // Hemisphere built on the XY disc (z = 0), bulging toward +Z — billboarded to camera each frame.
            function sph(lat, lon) {
                const cl = Math.cos(lat);
                return {
                    x: cl * Math.cos(lon),
                    y: cl * Math.sin(lon),
                    z: Math.sin(lat),
                };
            }

            const v00 = sph(lat0, lon0);
            const v01 = sph(lat0, lon1);
            const v10 = sph(lat1, lon0);
            const v11 = sph(lat1, lon1);

            const t0 = Math.sin(lat0);
            const t1 = Math.sin(lat1);
            const c00 = { r: lerp(br, hr, t0), g: lerp(bg, hg, t0), b: lerp(bb, hb, t0) };
            const c01 = { r: lerp(br, hr, t0), g: lerp(bg, hg, t0), b: lerp(bb, hb, t0) };
            const c10 = { r: lerp(br, hr, t1), g: lerp(bg, hg, t1), b: lerp(bb, hb, t1) };
            const c11 = { r: lerp(br, hr, t1), g: lerp(bg, hg, t1), b: lerp(bb, hb, t1) };

            const n0 = normalize3(v00.x, v00.y, v00.z);
            const n1 = normalize3(v11.x, v11.y, v11.z);
            const cr = (c00.r + c01.r + c10.r + c11.r) * 0.25f;
            const cg = (c00.g + c01.g + c10.g + c11.g) * 0.25f;
            const cb = (c00.b + c01.b + c10.b + c11.b) * 0.25f;

            this.tri(v00, v01, v11, n0.x, n0.y, n0.z, cr, cg, cb, 1.0f);
            this.tri(v00, v11, v10, n1.x, n1.y, n1.z, cr, cg, cb, 1.0f);
        }
    }
};

MeshBuilder.prototype.voxelFace = function (cx, cy, cz, hs, face, r, g, b, a) {
    if (face === 0) {
        this.quad(
            { x: cx + hs, y: cy - hs, z: cz - hs },
            { x: cx + hs, y: cy + hs, z: cz - hs },
            { x: cx + hs, y: cy + hs, z: cz + hs },
            { x: cx + hs, y: cy - hs, z: cz + hs },
            1.0, 0.0, 0.0, r, g, b, a
        );
    } else if (face === 1) {
        this.quad(
            { x: cx - hs, y: cy + hs, z: cz - hs },
            { x: cx - hs, y: cy - hs, z: cz - hs },
            { x: cx - hs, y: cy - hs, z: cz + hs },
            { x: cx - hs, y: cy + hs, z: cz + hs },
            -1.0, 0.0, 0.0, r, g, b, a
        );
    } else if (face === 2) {
        this.quad(
            { x: cx - hs, y: cy + hs, z: cz - hs },
            { x: cx + hs, y: cy + hs, z: cz - hs },
            { x: cx + hs, y: cy + hs, z: cz + hs },
            { x: cx - hs, y: cy + hs, z: cz + hs },
            0.0, 1.0, 0.0, r, g, b, a
        );
    } else if (face === 3) {
        this.quad(
            { x: cx + hs, y: cy - hs, z: cz - hs },
            { x: cx - hs, y: cy - hs, z: cz - hs },
            { x: cx - hs, y: cy - hs, z: cz + hs },
            { x: cx + hs, y: cy - hs, z: cz + hs },
            0.0, -1.0, 0.0, r, g, b, a
        );
    } else if (face === 4) {
        this.quad(
            { x: cx - hs, y: cy - hs, z: cz + hs },
            { x: cx + hs, y: cy - hs, z: cz + hs },
            { x: cx + hs, y: cy + hs, z: cz + hs },
            { x: cx - hs, y: cy + hs, z: cz + hs },
            0.0, 0.0, 1.0, r, g, b, a
        );
    } else {
        this.quad(
            { x: cx + hs, y: cy - hs, z: cz - hs },
            { x: cx - hs, y: cy - hs, z: cz - hs },
            { x: cx - hs, y: cy + hs, z: cz - hs },
            { x: cx + hs, y: cy + hs, z: cz - hs },
            0.0, 0.0, -1.0, r, g, b, a
        );
    }
};

function buildMeshData(builder, culling) {
    const positions = new Float32Array(builder.positions);
    const normals = new Float32Array(builder.normals);
    const texcoords = new Float32Array(builder.texcoords);
    const colors = new Float32Array(builder.colors);
    const vertexCount = positions.length / 4;

    const zero = Render.materialColor(0.0, 0.0, 0.0);
    const material = Render.material(
        zero, zero, zero, zero, zero,
        16.0f, 1.0f, zero, 0.0f,
        -1, -1, -1, -1
    );

    const vertList = Render.vertexList(
        positions, normals, texcoords, colors,
        [material], [Render.materialIndex(0, vertexCount)],
        { shareBuffers: true }
    );

    const data = new RenderData(vertList);
    data.updateMaterial(0, { diffuse: { r: 0.0f, g: 0.0f, b: 0.0f } });
    data.pipeline = Render.PL_NO_LIGHTS;
    data.face_culling = (culling === undefined) ? Render.CULL_FACE_NONE : culling;
    return data;
}

// ---------------------------------------------------------------------------
// Voxel asteroids — procedural grid, mesh rebuilt when voxels are carved away
// ---------------------------------------------------------------------------
function VoxelAsteroid(seed) {
    this.seed = seed;
    this.grid = new Uint8Array(VOXEL_GRID * VOXEL_GRID * VOXEL_GRID);
    this.voxelCount = 0;
    this.meshData = null;
    this.reset(seed);
}

VoxelAsteroid.prototype.idx = function (x, y, z) {
    return x + y * VOXEL_GRID + z * VOXEL_GRID * VOXEL_GRID;
};

VoxelAsteroid.prototype.get = function (x, y, z) {
    if (x < 0 || y < 0 || z < 0 || x >= VOXEL_GRID || y >= VOXEL_GRID || z >= VOXEL_GRID) return 0;
    return this.grid[this.idx(x, y, z)];
};

VoxelAsteroid.prototype.set = function (x, y, z, v) {
    if (x < 0 || y < 0 || z < 0 || x >= VOXEL_GRID || y >= VOXEL_GRID || z >= VOXEL_GRID) return;
    const i = this.idx(x, y, z);
    if (v && !this.grid[i]) this.voxelCount++;
    else if (!v && this.grid[i]) this.voxelCount--;
    this.grid[i] = v ? 1 : 0;
};

VoxelAsteroid.prototype.reset = function (seed) {
    this.seed = seed;
    this.grid.fill(0);
    this.voxelCount = 0;

    const cx = Math.floor(VOXEL_GRID * 0.5f);
    const cy = cx;
    const cz = cx;
    const target = 6 + (voxelHash(cx, cy, cz, seed) % (VOXEL_MAX - 5));

    this.set(cx, cy, cz, 1);

    const dirs = [
        [1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1],
    ];
    let fails = 0;

    while (this.voxelCount < target && fails < 80) {
        let pick = voxelHash(seed, this.voxelCount, fails) % this.voxelCount;
        let found = false;
        let ox = 0, oy = 0, oz = 0;

        for (let z = 0; z < VOXEL_GRID && !found; z++) {
            for (let y = 0; y < VOXEL_GRID && !found; y++) {
                for (let x = 0; x < VOXEL_GRID; x++) {
                    if (!this.get(x, y, z)) continue;
                    if (pick === 0) {
                        ox = x; oy = y; oz = z;
                        found = true;
                        break;
                    }
                    pick--;
                }
            }
        }
        if (!found) break;

        const di = voxelHash(ox, oy, oz, seed + fails) % dirs.length;
        const nx = ox + dirs[di][0];
        const ny = oy + dirs[di][1];
        const nz = oz + dirs[di][2];

        if (this.get(nx, ny, nz)) {
            fails++;
            continue;
        }
        if (nx < 0 || ny < 0 || nz < 0 || nx >= VOXEL_GRID || ny >= VOXEL_GRID || nz >= VOXEL_GRID) {
            fails++;
            continue;
        }

        this.set(nx, ny, nz, 1);
        fails = 0;
    }
    this.rebuildMesh();
};

VoxelAsteroid.prototype.collisionRadius = function () {
    if (this.voxelCount <= 0) return 0.0f;

    let maxDist = 0.0f;
    const corner = VOXEL_CELL * 0.866f;

    for (let z = 0; z < VOXEL_GRID; z++) {
        for (let y = 0; y < VOXEL_GRID; y++) {
            for (let x = 0; x < VOXEL_GRID; x++) {
                if (!this.get(x, y, z)) continue;
                const p = this.localFromGrid(x, y, z);
                const d = Math.sqrt(p.x * p.x + p.y * p.y + p.z * p.z) + corner;
                if (d > maxDist) maxDist = d;
            }
        }
    }
    return maxDist;
};

VoxelAsteroid.prototype.localFromGrid = function (ix, iy, iz) {
    const half = VOXEL_GRID * VOXEL_CELL * 0.5f;
    return {
        x: (ix + 0.5f) * VOXEL_CELL - half,
        y: (iy + 0.5f) * VOXEL_CELL - half,
        z: (iz + 0.5f) * VOXEL_CELL - half,
    };
};

VoxelAsteroid.prototype.gridFromLocal = function (lx, ly, lz) {
    const half = VOXEL_GRID * VOXEL_CELL * 0.5f;
    return {
        x: Math.floor((lx + half) / VOXEL_CELL),
        y: Math.floor((ly + half) / VOXEL_CELL),
        z: Math.floor((lz + half) / VOXEL_CELL),
    };
};

VoxelAsteroid.prototype.buildMeshBuilder = function () {
    const b = new MeshBuilder();
    const hs = VOXEL_CELL * 0.5f;
    const half = VOXEL_GRID * VOXEL_CELL * 0.5f;
    const grid = this.grid;
    const seed = this.seed;
    const G = VOXEL_GRID;
    const GG = G * G;

    // Direct grid indexing + inline bounds checks (no per-voxel get()/localFromGrid allocs).
    for (let z = 0; z < G; z++) {
        const zBase = z * GG;
        const zp = (z + 0.5f) * VOXEL_CELL - half;
        for (let y = 0; y < G; y++) {
            const rowBase = zBase + y * G;
            const yp = (y + 0.5f) * VOXEL_CELL - half;
            for (let x = 0; x < G; x++) {
                if (!grid[rowBase + x]) continue;
                const xp = (x + 0.5f) * VOXEL_CELL - half;
                const c = voxelColor(x, y, z, seed);

                if (x + 1 >= G || !grid[rowBase + x + 1]) b.voxelFace(xp, yp, zp, hs, 0, c.r, c.g, c.b, 1.0f);
                if (x - 1 < 0 || !grid[rowBase + x - 1]) b.voxelFace(xp, yp, zp, hs, 1, c.r, c.g, c.b, 1.0f);
                if (y + 1 >= G || !grid[rowBase + G + x]) b.voxelFace(xp, yp, zp, hs, 2, c.r, c.g, c.b, 1.0f);
                if (y - 1 < 0 || !grid[rowBase - G + x]) b.voxelFace(xp, yp, zp, hs, 3, c.r, c.g, c.b, 1.0f);
                if (z + 1 >= G || !grid[rowBase + GG + x]) b.voxelFace(xp, yp, zp, hs, 4, c.r, c.g, c.b, 1.0f);
                if (z - 1 < 0 || !grid[rowBase - GG + x]) b.voxelFace(xp, yp, zp, hs, 5, c.r, c.g, c.b, 1.0f);
            }
        }
    }
    return b;
};

VoxelAsteroid.prototype.rebuildMesh = function () {
    const b = this.buildMeshBuilder();
    if (b.positions.length === 0) {
        if (this.meshData) {
            this.meshData.vertices = {
                positions: new Float32Array(0),
                normals: new Float32Array(0),
                texcoords: new Float32Array(0),
                colors: new Float32Array(0),
            };
            this.meshData.material_indices = [Render.materialIndex(0, 0)];
        }
        return;
    }

    const positions = new Float32Array(b.positions);
    const normals = new Float32Array(b.normals);
    const texcoords = new Float32Array(b.texcoords);
    const colors = new Float32Array(b.colors);
    const vertexCount = positions.length / 4;

    if (!this.meshData) {
        const zero = Render.materialColor(0.0, 0.0, 0.0);
        const material = Render.material(
            zero, zero, zero, zero, zero,
            16.0f, 1.0f, zero, 0.0f,
            -1, -1, -1, -1
        );
        const vertList = Render.vertexList(
            positions, normals, texcoords, colors,
            [material], [Render.materialIndex(0, vertexCount)],
            { shareBuffers: true }
        );
        this.meshData = new RenderData(vertList);
        this.meshData.updateMaterial(0, { diffuse: { r: 0.0f, g: 0.0f, b: 0.0f } });
        this.meshData.pipeline = Render.PL_NO_LIGHTS;
        this.meshData.face_culling = Render.CULL_FACE_NONE;
        return;
    }

    this.meshData.vertices = {
        positions: positions,
        normals: normals,
        texcoords: texcoords,
        colors: colors,
    };
    this.meshData.material_indices = [Render.materialIndex(0, vertexCount)];
};

VoxelAsteroid.prototype.carveWorld = function (wx, wy, wz, ex, ey, ez, sc, rx, ry, rz, dmg) {
    dmg = dmg || 1;
    const local = worldToLocal(wx, wy, wz, ex, ey, ez, sc, rx, ry, rz);
    const hitSlop2 = VOXEL_CELL * VOXEL_CELL * (2.8f + (dmg - 1) * 1.4f);
    let removed = 0;
    let debris = 0;
    let best = null;
    let bestD2 = 1e9;

    for (let z = 0; z < VOXEL_GRID; z++) {
        for (let y = 0; y < VOXEL_GRID; y++) {
            for (let x = 0; x < VOXEL_GRID; x++) {
                if (!this.get(x, y, z)) continue;
                const p = this.localFromGrid(x, y, z);
                const dx = p.x - local.x;
                const dy = p.y - local.y;
                const dz = p.z - local.z;
                const d2 = dx * dx + dy * dy + dz * dz;
                if (d2 < bestD2) {
                    bestD2 = d2;
                    best = { x: x, y: y, z: z, p: p };
                }
            }
        }
    }

    if (!best) return 0;

    function removeAt(self, x, y, z, p, ex, ey, ez, sc, rx, ry, rz, debrisRef) {
        if (!self.get(x, y, z)) return 0;
        if (debrisRef.n < 6) {
            const world = localToWorld(p.x, p.y, p.z, ex, ey, ez, sc, rx, ry, rz);
            spawnVoxelDebris(world.x, world.y, world.z);
            debrisRef.n++;
        }
        self.set(x, y, z, 0);
        return 1;
    }

    const debrisRef = { n: debris };

    for (let z = 0; z < VOXEL_GRID; z++) {
        for (let y = 0; y < VOXEL_GRID; y++) {
            for (let x = 0; x < VOXEL_GRID; x++) {
                if (!this.get(x, y, z)) continue;
                const p = this.localFromGrid(x, y, z);
                const dx = p.x - local.x;
                const dy = p.y - local.y;
                const dz = p.z - local.z;
                const d2 = dx * dx + dy * dy + dz * dz;
                if (d2 <= hitSlop2) {
                    removed += removeAt(this, x, y, z, p, ex, ey, ez, sc, rx, ry, rz, debrisRef);
                }
            }
        }
    }

    if (removed === 0) {
        removed += removeAt(this, best.x, best.y, best.z, best.p, ex, ey, ez, sc, rx, ry, rz, debrisRef);
    }

    for (let extra = 1; extra < dmg && this.voxelCount > VOXEL_DESTROY_AT; extra++) {
        let near = null;
        let nearD2 = 1e9;
        for (let z = 0; z < VOXEL_GRID; z++) {
            for (let y = 0; y < VOXEL_GRID; y++) {
                for (let x = 0; x < VOXEL_GRID; x++) {
                    if (!this.get(x, y, z)) continue;
                    const p = this.localFromGrid(x, y, z);
                    const dx = p.x - local.x;
                    const dy = p.y - local.y;
                    const dz = p.z - local.z;
                    const d2 = dx * dx + dy * dy + dz * dz;
                    if (d2 < nearD2) {
                        nearD2 = d2;
                        near = { x: x, y: y, z: z, p: p };
                    }
                }
            }
        }
        if (!near) break;
        removed += removeAt(this, near.x, near.y, near.z, near.p, ex, ey, ez, sc, rx, ry, rz, debrisRef);
    }

    if (removed > 0) this.rebuildMesh();
    return removed;
};

function spawnVoxelDebris(x, y, z) {
    const ang = Math.randomf(0.0f, 6.28f);
    const spd = Math.randomf(0.06f, 0.22f);
    spawnParticle(
        2,
        x + Math.randomf(-0.06f, 0.06f),
        y + Math.randomf(-0.06f, 0.06f),
        z + Math.randomf(-0.06f, 0.06f),
        Math.cos(ang) * spd,
        Math.randomf(0.02f, 0.14f),
        Math.sin(ang) * spd,
        Math.randomi(12, 24),
        Math.randomf(0.25f, 0.55f)
    );
}

// ---------------------------------------------------------------------------
// Meshes
// ---------------------------------------------------------------------------
function buildThruster() {
    const b = new MeshBuilder();
    b.lit = false;
    b.box(0.0f, 0.0f, 0.0f, 0.24f, 0.18f, 0.5f, 1.0f, 0.6f, 0.15f, 1.0f);
    b.box(0.0f, 0.0f, -0.28f, 0.12f, 0.1f, 0.4f, 1.0f, 0.9f, 0.5f, 1.0f);
    return buildMeshData(b);
}

function buildEnemy() {
    const b = new MeshBuilder();
    b.box(0.0f, 0.0f, 0.0f, 0.85f, 0.42f, 0.9f, 0.85f, 0.2f, 0.18f, 1.0f);     // body
    b.box(0.0f, 0.0f, -0.5f, 0.32f, 0.32f, 0.45f, 1.0f, 0.45f, 0.15f, 1.0f);   // front intake
    b.box(0.0f, 0.14f, 0.35f, 0.22f, 0.16f, 0.4f, 0.55f, 0.12f, 0.12f, 1.0f);  // hump
    b.box(-0.85f, 0.0f, 0.0f, 0.7f, 0.08f, 0.5f, 0.72f, 0.18f, 0.16f, 1.0f);   // left wing
    b.box(0.85f, 0.0f, 0.0f, 0.7f, 0.08f, 0.5f, 0.72f, 0.18f, 0.16f, 1.0f);    // right wing
    b.box(-1.2f, 0.0f, 0.0f, 0.22f, 0.28f, 0.3f, 0.95f, 0.75f, 0.15f, 1.0f);   // left tip
    b.box(1.2f, 0.0f, 0.0f, 0.22f, 0.28f, 0.3f, 0.95f, 0.75f, 0.15f, 1.0f);    // right tip
    return buildMeshData(b);
}

function buildBullet() {
    const b = new MeshBuilder();
    b.lit = false;
    b.box(0.0f, 0.0f, 0.0f, 0.12f, 0.12f, 1.1f, 0.35f, 1.0f, 0.55f, 1.0f);
    b.box(0.0f, 0.0f, 0.0f, 0.05f, 0.05f, 0.5f, 0.9f, 1.0f, 0.9f, 1.0f);
    return buildMeshData(b);
}

function buildExplosion() {
    const b = new MeshBuilder();
    b.lit = false;
    b.box(0.0f, 0.0f, 0.0f, 0.28f, 0.28f, 0.28f, 1.0f, 0.98f, 0.88f, 1.0f);
    b.box(0.0f, 0.0f, 0.0f, 0.62f, 0.62f, 0.62f, 1.0f, 0.82f, 0.18f, 1.0f);
    b.box(0.0f, 0.0f, 0.0f, 0.95f, 0.95f, 0.95f, 0.85f, 0.28f, 0.08f, 1.0f);
    return buildMeshData(b);
}

function buildStar() {
    const b = new MeshBuilder();
    b.lit = false;
    b.box(0.0f, 0.0f, 0.0f, 0.42f, 0.42f, 0.42f, 1.0f, 1.0f, 1.0f, 1.0f);
    return buildMeshData(b);
}

function buildPlanetDome(br, bg, bb, hr, hg, hb) {
    const b = new MeshBuilder();
    b.lit = false;
    b.dome(br, bg, bb, hr, hg, hb, 10, 18);
    return buildMeshData(b);
}

function buildParticle(kind) {
    const b = new MeshBuilder();
    b.lit = false;
    if (kind === "boost") {
        b.box(0.0f, 0.0f, 0.0f, 0.06f, 0.06f, 0.35f, 1.0f, 0.72f, 0.18f, 1.0f);
        b.box(0.0f, 0.0f, -0.12f, 0.03f, 0.03f, 0.22f, 1.0f, 0.95f, 0.55f, 1.0f);
    } else if (kind === "spark") {
        b.box(0.0f, 0.0f, 0.0f, 0.025f, 0.025f, 0.42f, 1.0f, 0.95f, 0.55f, 1.0f);
        b.box(0.0f, 0.0f, 0.0f, 0.012f, 0.012f, 0.65f, 1.0f, 1.0f, 0.85f, 1.0f);
    } else if (kind === "flash") {
        b.box(0.0f, 0.0f, 0.0f, 0.07f, 0.07f, 0.07f, 1.0f, 0.98f, 0.82f, 1.0f);
        b.box(0.0f, 0.0f, 0.0f, 0.03f, 0.03f, 0.28f, 1.0f, 1.0f, 0.92f, 1.0f);
    } else if (kind === "smoke") {
        b.box(0.0f, 0.0f, 0.0f, 0.09f, 0.09f, 0.09f, 0.35f, 0.32f, 0.38f, 1.0f);
        b.box(0.0f, 0.0f, 0.0f, 0.06f, 0.06f, 0.06f, 0.22f, 0.20f, 0.24f, 1.0f);
    } else {
        b.box(0.0f, 0.0f, 0.0f, 0.06f, 0.06f, 0.06f, 1.0f, 0.62f, 0.18f, 1.0f);
        b.box(0.0f, 0.0f, 0.0f, 0.04f, 0.04f, 0.04f, 0.85f, 0.45f, 0.12f, 1.0f);
    }
    return buildMeshData(b);
}

const save = loadSave();
const thrusterMesh = buildThruster();
const enemyMesh = buildEnemy();
const bulletMesh = buildBullet();
const explosionMesh = buildExplosion();
const starMesh = buildStar();
const boostParticleMesh = buildParticle("boost");
const sparkParticleMesh = buildParticle("spark");
const emberParticleMesh = buildParticle("ember");
const flashParticleMesh = buildParticle("flash");
const smokeParticleMesh = buildParticle("smoke");

const planetDefs = [
    { br: 0.22f, bg: 0.16f, bb: 0.42f, hr: 0.55f, hg: 0.42f, hb: 0.82f, x: -34.0f, y: 3.5f, z: 165.0f, sc: 2.4f },
    { br: 0.18f, bg: 0.22f, bb: 0.38f, hr: 0.38f, hg: 0.48f, hb: 0.72f, x: 38.0f, y: 11.0f, z: 235.0f, sc: 1.8f },
    { br: 0.32f, bg: 0.14f, bb: 0.12f, hr: 0.72f, hg: 0.38f, hb: 0.22f, x: 14.0f, y: 14.5f, z: 145.0f, sc: 1.4f },
    { br: 0.12f, bg: 0.18f, bb: 0.28f, hr: 0.28f, hg: 0.36f, hb: 0.58f, x: -22.0f, y: 7.5f, z: 205.0f, sc: 1.1f },
];
const planetMeshes = planetDefs.map(function (p) {
    return buildPlanetDome(p.br, p.bg, p.bb, p.hr, p.hg, p.hb);
});

// ---------------------------------------------------------------------------
// Scene objects (RenderObjects are reused; positions are ASSIGNED each frame)
// ---------------------------------------------------------------------------
let ship = null;
rebuildShip();
const thruster = new RenderObject(thrusterMesh);

// Distant starfield: far away, drifts laterally (parallax) instead of flying past the camera.
const stars = [];
for (let i = 0; i < STAR_COUNT; i++) {
    stars.push({
        obj: new RenderObject(starMesh),
        x: Math.randomf(-STAR_SPREAD_X, STAR_SPREAD_X),
        y: Math.randomf(-STAR_SPREAD_Y, STAR_SPREAD_Y),
        z: Math.randomf(STAR_Z_NEAR, STAR_Z_FAR),
        par: Math.randomf(0.04f, 0.16f),
        sc: Math.randomf(0.35f, 0.95f),
    });
}

const planets = [];
for (let i = 0; i < MAX_PLANETS; i++) {
    const def = planetDefs[i];
    planets.push({
        obj: new RenderObject(planetMeshes[i]),
        x: def.x,
        y: def.y,
        z: def.z,
        sc: def.sc,
    });
}

const particles = [];
for (let i = 0; i < MAX_PARTICLES; i++) {
    let kind = 0;
    if (i >= 18 && i < 40) kind = 1;
    else if (i >= 40 && i < 54) kind = 2;
    else if (i >= 54 && i < 66) kind = 3;
    else if (i >= 66) kind = 4;

    let mesh = boostParticleMesh;
    if (kind === 1) mesh = sparkParticleMesh;
    else if (kind === 2) mesh = emberParticleMesh;
    else if (kind === 3) mesh = flashParticleMesh;
    else if (kind === 4) mesh = smokeParticleMesh;

    particles.push({
        obj: new RenderObject(mesh),
        slotKind: kind,
        alive: false,
        x: 0, y: 0, z: 0,
        vx: 0, vy: 0, vz: 0,
        life: 0, maxLife: 1,
        sc: 1.0f,
        spin: 0.0f,
    });
}

const bullets = [];
for (let i = 0; i < MAX_BULLETS; i++) {
    bullets.push({ obj: new RenderObject(bulletMesh), alive: false, x: 0, y: 0, z: 0, pz: 0, damage: 1 });
}

const enemyPool = [];
for (let i = 0; i < MAX_ENEMIES; i++) {
    const voxel = new VoxelAsteroid(1000 + i * 7919);
    enemyPool.push({
        objFighter: new RenderObject(enemyMesh),
        objRock: new RenderObject(voxel.meshData),
        voxel: voxel,
        active: false,
        type: 0, x: 0, y: 0, z: 0, rx: 0, ry: 0, rz: 0,
        spin: 0, weave: 0, phase: 0, hp: 1, radius: 0.9, sc: 1.0,
    });
}

const MAX_FX = 12;
const fx = [];
for (let i = 0; i < MAX_FX; i++) {
    fx.push({ obj: new RenderObject(explosionMesh), alive: false, x: 0, y: 0, z: 0, life: 0, maxLife: 1, baseSc: 1.0f, spin: 0.0f });
}

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
let state = STATE_MENU;
let score = 0;
let lives = 3;
let wave = 1;
let fireTimer = 0;
let spawnTimer = 60;
let invuln = 0;
let fireSide = 1;
let shake = 0.0f;
let frame = 0;
let menuSpin = 0.0f;
let runStats = computeStats(save);
let runEarned = 0;
let hangTab = 0;
let hangSel = 0;
let partSlot = 0;
let hangNavLock = 0;

let shipX = 0.0f, shipY = 0.0f;
let velX = 0.0f, velY = 0.0f;
let roll = 0.0f, pitch = 0.0f;
let speed = BASE_SPEED;

const pad = Pads.get();

// Camera.target() in Athena pans by delta each call — do NOT invoke it every frame.
const CAM_X = 0.0f;
const CAM_Y = 1.4f;
const CAM_Z = -9.0f;
const LOOK_X = 0.0f;
const LOOK_Y = 0.0f;
const LOOK_Z = 24.0f;

function stick(v) {
    if (v > -28 && v < 28) return 0.0f;
    return clamp(v / 128.0f, -1.0f, 1.0f);
}

function resetGame() {
    runStats = computeStats(save);
    score = 0;
    lives = runStats.lives;
    wave = 1;
    fireTimer = 0;
    spawnTimer = 45;
    invuln = 0;
    shake = 0.0f;
    shipX = 0.0f; shipY = 0.0f;
    velX = 0.0f; velY = 0.0f;
    roll = 0.0f; pitch = 0.0f;
    speed = BASE_SPEED * runStats.speed;
    runEarned = 0;

    for (let i = 0; i < bullets.length; i++) bullets[i].alive = false;
    for (let i = 0; i < enemyPool.length; i++) enemyPool[i].active = false;
    for (let i = 0; i < fx.length; i++) fx[i].alive = false;
    for (let i = 0; i < particles.length; i++) particles[i].alive = false;
    for (let i = 0; i < stars.length; i++) {
        stars[i].x = Math.randomf(-STAR_SPREAD_X, STAR_SPREAD_X);
        stars[i].y = Math.randomf(-STAR_SPREAD_Y, STAR_SPREAD_Y);
        stars[i].z = Math.randomf(STAR_Z_NEAR, STAR_Z_FAR);
    }
    for (let i = 0; i < planets.length; i++) {
        const def = planetDefs[i];
        planets[i].x = def.x;
        planets[i].y = def.y;
        planets[i].z = def.z;
    }
}

function spawnEnemy() {
    for (let i = 0; i < enemyPool.length; i++) {
        const e = enemyPool[i];
        if (e.active) continue;
        e.active = true;
        e.type = Math.randomi(0, 100) < 32 ? 1 : 0;
        e.x = Math.randomf(-BOUNDS_X, BOUNDS_X);
        e.y = Math.randomf(BOUNDS_Y_LOW + 0.5f, BOUNDS_Y_HIGH - 0.5f);
        e.z = Math.randomf(95.0f, 130.0f);
        e.rx = Math.randomf(0.0f, 6.28f);
        e.ry = Math.randomf(0.0f, 6.28f);
        e.rz = Math.randomf(0.0f, 6.28f);
        e.spin = Math.randomf(-0.05f, 0.05f);
        e.weave = Math.randomf(0.008f, 0.03f);
        e.phase = Math.randomf(0.0f, 6.28f);
        if (e.type === 1) {
            e.sc = Math.randomf(0.85f, 1.25f);
            e.voxel.reset(Math.randomi(1, 999999));
            e.radius = e.voxel.collisionRadius() * e.sc;
            e.hp = 1;
        } else {
            e.hp = 1;
            e.radius = 0.9f;
            e.sc = 1.0f;
        }
        return;
    }
}

function awardRunCredits() {
    runEarned = Math.floor(score / 10) + wave * 25;
    save.credits += runEarned;
    if (score > save.best) save.best = score;
    saveGame();
}

function fireBullet() {
    const n = runStats.bullets;
    const spread = runStats.spread;
    for (let b = 0; b < n; b++) {
        const t = (n === 1) ? 0.5f : b / (n - 1);
        const off = (t - 0.5f) * spread * 2.0f;
        const wx = (fireSide > 0 ? 1.6f : -1.6f) + off;
        for (let i = 0; i < bullets.length; i++) {
            if (bullets[i].alive) continue;
            bullets[i].alive = true;
            bullets[i].x = shipX + wx;
            bullets[i].y = shipY;
            bullets[i].z = 1.2f;
            bullets[i].pz = 1.2f;
            bullets[i].damage = runStats.damage;
            break;
        }
    }
    fireSide = -fireSide;
}

function spawnFx(x, y, z, big) {
    for (let i = 0; i < fx.length; i++) {
        if (fx[i].alive) continue;
        fx[i].alive = true;
        fx[i].x = x; fx[i].y = y; fx[i].z = z;
        fx[i].maxLife = big ? 30 : 20;
        fx[i].life = fx[i].maxLife;
        fx[i].baseSc = big ? 0.45f : 0.28f;
        fx[i].spin = Math.randomf(-0.25f, 0.25f);
        spawnExplosionParticles(x, y, z, big);
        if (big) shake = Math.max(shake, 0.55f);
        return;
    }
}

function spawnParticle(kind, x, y, z, vx, vy, vz, life, sc, spin) {
    for (let i = 0; i < particles.length; i++) {
        const p = particles[i];
        if (p.alive || p.slotKind !== kind) continue;
        p.alive = true;
        p.x = x; p.y = y; p.z = z;
        p.vx = vx; p.vy = vy; p.vz = vz;
        p.maxLife = life;
        p.life = life;
        p.sc = sc;
        p.spin = (spin === undefined) ? Math.randomf(-0.4f, 0.4f) : spin;
        return true;
    }
    return false;
}

function burstDir(speed) {
    const yaw = Math.randomf(0.0f, 6.283185f);
    const pitch = Math.randomf(-0.95f, 0.95f);
    const cp = Math.cos(pitch);
    return {
        x: Math.cos(yaw) * cp * speed,
        y: Math.sin(pitch) * speed,
        z: Math.sin(yaw) * cp * speed,
    };
}

function spawnExplosionParticles(x, y, z, big) {
    const flashCount = big ? 8 : 4;
    const sparkCount = big ? 18 : 10;
    const emberCount = big ? 10 : 6;
    const smokeCount = big ? 6 : 3;

    for (let n = 0; n < flashCount; n++) {
        const d = burstDir(Math.randomf(0.04f, big ? 0.18f : 0.12f));
        spawnParticle(
            3,
            x + Math.randomf(-0.08f, 0.08f),
            y + Math.randomf(-0.08f, 0.08f),
            z + Math.randomf(-0.08f, 0.08f),
            d.x, d.y, d.z,
            Math.randomi(6, 12),
            Math.randomf(0.7f, 1.3f)
        );
    }

    for (let n = 0; n < sparkCount; n++) {
        const spd = Math.randomf(big ? 0.22f : 0.14f, big ? 0.58f : 0.38f);
        const d = burstDir(spd);
        spawnParticle(
            1,
            x + Math.randomf(-0.12f, 0.12f),
            y + Math.randomf(-0.12f, 0.12f),
            z + Math.randomf(-0.12f, 0.12f),
            d.x, d.y, d.z,
            Math.randomi(14, big ? 28 : 22),
            Math.randomf(0.45f, 1.0f)
        );
    }

    for (let n = 0; n < emberCount; n++) {
        const d = burstDir(Math.randomf(0.08f, big ? 0.32f : 0.22f));
        d.y += Math.randomf(0.02f, 0.12f);
        spawnParticle(
            2,
            x + Math.randomf(-0.16f, 0.16f),
            y + Math.randomf(-0.16f, 0.16f),
            z + Math.randomf(-0.16f, 0.16f),
            d.x, d.y, d.z,
            Math.randomi(18, big ? 36 : 28),
            Math.randomf(0.35f, 0.85f)
        );
    }

    for (let n = 0; n < smokeCount; n++) {
        const d = burstDir(Math.randomf(0.03f, big ? 0.14f : 0.09f));
        d.y += Math.randomf(0.04f, 0.16f);
        spawnParticle(
            4,
            x + Math.randomf(-0.1f, 0.1f),
            y + Math.randomf(-0.1f, 0.1f),
            z + Math.randomf(-0.1f, 0.1f),
            d.x, d.y * 0.6f, d.z,
            Math.randomi(22, big ? 40 : 32),
            Math.randomf(0.55f, big ? 1.4f : 1.0f)
        );
    }
}

function spawnBoostParticles(sx, sy, sz, boostPower) {
    const count = boostPower > 0.5f ? 3 : 1;
    for (let n = 0; n < count; n++) {
        spawnParticle(
            0,
            sx + Math.randomf(-0.12f, 0.12f),
            sy + Math.randomf(-0.08f, 0.08f),
            sz + Math.randomf(-0.05f, 0.05f),
            Math.randomf(-0.04f, 0.04f),
            Math.randomf(-0.03f, 0.03f),
            -Math.randomf(0.35f, 0.75f) - boostPower * 0.4f,
            Math.randomi(8, 16),
            Math.randomf(0.5f, 1.1f) + boostPower * 0.5f
        );
    }
}

function hit(ax, ay, az, ar, bx, by, bz, br) {
    const dx = ax - bx, dy = ay - by, dz = az - bz;
    const rr = ar + br;
    return (dx * dx + dy * dy + dz * dz) < rr * rr;
}

function hitBulletAlongZ(bx, by, bz, prevZ, br, ex, ey, ez, er) {
    const steps = 6;
    const z0 = prevZ;
    for (let s = 0; s <= steps; s++) {
        const z = (s === steps) ? bz : lerp(z0, bz, s / steps);
        if (hit(bx, by, z, br, ex, ey, ez, er)) return z;
    }
    return -1.0f;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
function updateWorld(scrollSpeed) {
    // Distant stars stay far away and slide sideways (parallax), never approaching the camera.
    for (let i = 0; i < stars.length; i++) {
        const s = stars[i];
        s.x -= scrollSpeed * s.par;
        if (s.x < -STAR_SPREAD_X) {
            s.x += STAR_SPREAD_X * 2.0f;
            s.y = Math.randomf(-STAR_SPREAD_Y, STAR_SPREAD_Y);
            s.z = Math.randomf(STAR_Z_NEAR, STAR_Z_FAR);
        }
    }
    for (let i = 0; i < planets.length; i++) {
        planets[i].z -= scrollSpeed * 0.35f;
        if (planets[i].z < 60.0f) {
            const def = planetDefs[i];
            planets[i].z += 260.0f;
            planets[i].x = def.x + Math.randomf(-8.0f, 8.0f);
            planets[i].y = def.y + Math.randomf(-2.5f, 2.5f);
        }
    }
}

function updatePlay() {
    const ix = stick(pad.lx);
    const iy = stick(pad.ly);
    const boosting = pad.pressed(Pads.R2);
    const braking = pad.pressed(Pads.L2);

    let targetSpeed = BASE_SPEED * runStats.speed + wave * 0.03f;
    if (boosting) targetSpeed *= 2.1f;
    else if (braking) targetSpeed *= 0.45f;
    speed = lerp(speed, targetSpeed, 0.08f);

    velX = lerp(velX, -ix * SHIP_ACCEL, 0.35f);
    velY = lerp(velY, iy * SHIP_ACCEL * 0.85f, 0.35f);
    shipX = clamp(shipX + velX, -BOUNDS_X, BOUNDS_X);
    shipY = clamp(shipY + velY, BOUNDS_Y_LOW, BOUNDS_Y_HIGH);

    roll = lerp(roll, ix * 0.7f, 0.2f);
    pitch = lerp(pitch, iy * 0.35f, 0.2f);

    if (fireTimer > 0) fireTimer--;
    if ((pad.pressed(Pads.CROSS) || pad.pressed(Pads.R1)) && fireTimer === 0) {
        fireBullet();
        fireTimer = runStats.cooldown;
    }

    updateWorld(speed);

    const boostPower = clamp((speed - BASE_SPEED) / (BASE_SPEED * 1.2f), 0.0f, 1.0f);
    if (boosting && state === STATE_PLAY) {
        spawnBoostParticles(shipX, shipY, -1.35f, boostPower);
    }

    spawnTimer--;
    if (spawnTimer <= 0) {
        spawnEnemy();
        spawnTimer = Math.max(14, 42 - wave * 2);
    }
    if (frame % 900 === 0 && frame > 0) wave++;

    for (let i = 0; i < bullets.length; i++) {
        if (!bullets[i].alive) continue;
        bullets[i].pz = bullets[i].z;
        bullets[i].z += 3.4f;
        if (bullets[i].z > 135.0f) bullets[i].alive = false;
    }

    for (let i = 0; i < enemyPool.length; i++) {
        const e = enemyPool[i];
        if (!e.active) continue;

        e.z -= speed * (e.type === 1 ? 0.7f : 1.0f) + 0.15f;
        e.phase += e.weave;
        e.x += Math.sin(e.phase) * 0.04f;
        e.ry += e.spin;
        e.rx += e.spin * 0.5f;

        if (e.z < -8.0f) { e.active = false; continue; }

        if (invuln <= 0 && hit(shipX, shipY, 0.0f, 0.8f, e.x, e.y, e.z, e.radius)) {
            e.active = false;
            lives--;
            invuln = 90;
            shake = 1.0f;
            spawnFx(shipX, shipY, 0.5f, true);
            if (lives <= 0) {
                awardRunCredits();
                state = STATE_OVER;
                return;
            }
            continue;
        }

        for (let j = 0; j < bullets.length; j++) {
            if (!bullets[j].alive) continue;

            let bx = bullets[j].x;
            let by = bullets[j].y;
            let bz = bullets[j].z;
            let hitZ = -1.0f;

            if (e.type === 1) {
                const br = 0.55f;
                const er = e.radius + 0.25f;
                hitZ = hitBulletAlongZ(bx, by, bz, bullets[j].pz, br, e.x, e.y, e.z, er);
            } else if (hit(bx, by, bz, 0.35f, e.x, e.y, e.z, e.radius)) {
                hitZ = bz;
            }

            if (hitZ < 0.0f) continue;

            bz = hitZ;
            bullets[j].alive = false;

            if (e.type === 1) {
                const removed = e.voxel.carveWorld(
                    bx, by, bz,
                    e.x, e.y, e.z, e.sc, e.rx, e.ry, e.rz,
                    bullets[j].damage || 1
                );
                spawnFx(bx, by, bz, false);
                score += removed * 8;
                e.radius = e.voxel.collisionRadius() * e.sc;

                if (e.voxel.voxelCount <= VOXEL_DESTROY_AT) {
                    e.active = false;
                    spawnFx(e.x, e.y, e.z, true);
                    score += 60;
                }
            } else {
                e.hp -= bullets[j].damage || 1;
                if (e.hp <= 0) {
                    e.active = false;
                    spawnFx(e.x, e.y, e.z, false);
                    score += 100;
                } else {
                    spawnFx(bx, by, bz, false);
                }
            }
            break;
        }
    }

    for (let i = 0; i < fx.length; i++) {
        if (!fx[i].alive) continue;
        fx[i].life--;
        if (fx[i].life <= 0) { fx[i].alive = false; continue; }
    }

    for (let i = 0; i < particles.length; i++) {
        const p = particles[i];
        if (!p.alive) continue;
        p.life--;
        p.x += p.vx;
        p.y += p.vy;
        p.z += p.vz;

        if (p.slotKind === 0) {
            p.vz -= 0.02f;
            p.sc = lerp(p.sc, 0.15f, 0.12f);
        } else if (p.slotKind === 1) {
            p.vx *= 0.90f;
            p.vy *= 0.90f;
            p.vz *= 0.90f;
            p.vy -= 0.008f;
            p.sc = lerp(p.sc, 0.08f, 0.10f);
        } else if (p.slotKind === 2) {
            p.vx *= 0.94f;
            p.vy *= 0.94f;
            p.vz *= 0.94f;
            p.vy -= 0.012f;
            p.sc = lerp(p.sc, 0.04f, 0.06f);
        } else if (p.slotKind === 3) {
            p.vx *= 0.86f;
            p.vy *= 0.86f;
            p.vz *= 0.86f;
            p.sc = lerp(p.sc, 0.02f, 0.18f);
        } else {
            p.vx *= 0.96f;
            p.vy *= 0.96f;
            p.vz *= 0.96f;
            p.vy += 0.006f;
            p.sc = lerp(p.sc, 1.6f, 0.04f);
        }

        if (p.life <= 0) p.alive = false;
    }

    if (invuln > 0) invuln--;
    frame++;
}

// ---------------------------------------------------------------------------
// Hangar hub
// ---------------------------------------------------------------------------
function hangarListCount() {
    if (hangTab === 0) return SHIP_IDS.length;
    if (hangTab === 1) return PARTS[PART_SLOTS[partSlot]].length;
    if (hangTab === 2) return COLORS.length;
    if (hangTab === 3) return UPGRADE_KEYS.length;
    return 0;
}

function hangarEquipShip(id) {
    save.loadout.ship = id;
    rebuildShip();
    saveGame();
}

function hangarBuyShip(id) {
    const hull = SHIPS[id];
    if (!hull || ownsShip(id)) {
        hangarEquipShip(id);
        return;
    }
    if (save.credits < hull.price) return;
    save.credits -= hull.price;
    save.ownedShips.push(id);
    hangarEquipShip(id);
}

function hangarBuyPart(slot, id) {
    const p = findPart(slot, id);
    if (ownsPart(slot, id)) {
        save.loadout[slot] = id;
        rebuildShip();
        saveGame();
        return;
    }
    if (save.credits < p.price) return;
    save.credits -= p.price;
    save.ownedParts[slot].push(id);
    save.loadout[slot] = id;
    rebuildShip();
    saveGame();
}

function hangarBuyColor(id) {
    const c = findColor(id);
    if (ownsColor(id)) {
        save.loadout.color = id;
        rebuildShip();
        saveGame();
        return;
    }
    if (save.credits < c.price) return;
    save.credits -= c.price;
    save.ownedColors.push(id);
    save.loadout.color = id;
    rebuildShip();
    saveGame();
}

function hangarBuyUpgrade(key) {
    const def = UPGRADE_DEFS[key];
    const lv = save.upgrades[key];
    if (lv >= def.max) return;
    const cost = upgradeCost(key, lv);
    if (save.credits < cost) return;
    save.credits -= cost;
    save.upgrades[key]++;
    saveGame();
}

function hangarConfirm() {
    if (hangTab === 0) {
        hangarBuyShip(SHIP_IDS[hangSel]);
    } else if (hangTab === 1) {
        const slot = PART_SLOTS[partSlot];
        hangarBuyPart(slot, PARTS[slot][hangSel].id);
    } else if (hangTab === 2) {
        hangarBuyColor(COLORS[hangSel].id);
    } else if (hangTab === 3) {
        hangarBuyUpgrade(UPGRADE_KEYS[hangSel]);
    } else if (hangTab === 4) {
        resetGame();
        state = STATE_PLAY;
    }
}

function updateHangar() {
    if (hangNavLock > 0) hangNavLock--;

    if (pad.justPressed(Pads.L1)) {
        hangTab = (hangTab + HANGAR_TABS.length - 1) % HANGAR_TABS.length;
        hangSel = 0;
        hangNavLock = 12;
    }
    if (pad.justPressed(Pads.R1)) {
        hangTab = (hangTab + 1) % HANGAR_TABS.length;
        hangSel = 0;
        hangNavLock = 12;
    }

    if (hangTab === 1) {
        if (pad.justPressed(Pads.LEFT)) {
            partSlot = (partSlot + PART_SLOTS.length - 1) % PART_SLOTS.length;
            hangSel = 0;
            hangNavLock = 12;
        }
        if (pad.justPressed(Pads.RIGHT)) {
            partSlot = (partSlot + 1) % PART_SLOTS.length;
            hangSel = 0;
            hangNavLock = 12;
        }
    }

    const count = hangarListCount();
    if (count > 0 && hangNavLock === 0) {
        if (pad.justPressed(Pads.UP)) {
            hangSel = (hangSel + count - 1) % count;
            hangNavLock = 10;
        }
        if (pad.justPressed(Pads.DOWN)) {
            hangSel = (hangSel + 1) % count;
            hangNavLock = 10;
        }
    }

    if (pad.justPressed(Pads.CROSS)) hangarConfirm();
}

function drawHangarUi() {
    const px = Math.floor(CW * 0.42f);
    const pw = CW - px - 10;

    Draw.rect(0, 0, px, CH, Color.new(0, 0, 0, 40));
    Draw.rect(px, 30, pw, CH - 38, Color.new(16, 22, 38, 200));

    let tx = px + 6;
    for (let t = 0; t < HANGAR_TABS.length; t++) {
        font.color = (t === hangTab) ? C_YELLOW : C_DIM;
        font.print(tx, 8, HANGAR_TABS[t]);
        tx += 86;
    }

    font.color = C_CYAN;
    font.print(px + 8, 38, `CREDITS ${save.credits}`);
    font.color = C_DIM;
    font.print(px + 8, 56, `BEST ${save.best}`);

    if (hangTab === 4) {
        const rs = computeStats(save);
        font.color = C_WHITE;
        font.print(px + 8, 88, "LAUNCH RUN");
        font.print(px + 8, 112, `${SHIPS[save.loadout.ship].name} / ${findColor(save.loadout.color).name}`);
        font.color = C_DIM;
        font.print(px + 8, 136, `Lives ${rs.lives}  Dmg ${rs.damage}  Spd x${Math.floor(rs.speed * 100) / 100}`);
        font.print(px + 8, 156, `Shots ${rs.bullets}  Rate ${rs.cooldown}f`);
        font.color = C_YELLOW;
        font.print(px + 8, 190, "CROSS = Launch");
        font.color = C_DIM;
        font.print(px + 8, 212, "L1/R1 tabs  TRIANGLE quit");
        return;
    }

    const listY = 78;
    const rowH = 22;
    const maxRows = Math.floor((CH - listY - 16) / rowH);

    if (hangTab === 1) {
        font.color = C_WHITE;
        font.print(px + 8, listY - 18, `Slot: ${PART_SLOTS[partSlot].toUpperCase()}  < >`);
    }

    for (let i = 0; i < maxRows; i++) {
        const idx = i;
        if (idx >= hangarListCount()) break;
        const y = listY + i * rowH;
        const sel = idx === hangSel;

        if (sel) Draw.rect(px + 4, y - 2, pw - 8, rowH - 2, Color.new(40, 60, 100, 160));

        let label = "";
        let price = 0;
        let owned = false;
        let equipped = false;

        if (hangTab === 0) {
            const id = SHIP_IDS[idx];
            const hull = SHIPS[id];
            label = hull.name;
            price = hull.price;
            owned = ownsShip(id);
            equipped = save.loadout.ship === id;
        } else if (hangTab === 1) {
            const slot = PART_SLOTS[partSlot];
            const p = PARTS[slot][idx];
            label = p.name;
            price = p.price;
            owned = ownsPart(slot, p.id);
            equipped = save.loadout[slot] === p.id;
        } else if (hangTab === 2) {
            const c = COLORS[idx];
            label = c.name;
            price = c.price;
            owned = ownsColor(c.id);
            equipped = save.loadout.color === c.id;
        } else if (hangTab === 3) {
            const key = UPGRADE_KEYS[idx];
            const def = UPGRADE_DEFS[key];
            const lv = save.upgrades[key];
            label = `${def.name} ${lv}/${def.max}`;
            price = (lv >= def.max) ? 0 : upgradeCost(key, lv);
            owned = lv >= def.max;
            equipped = false;
        }

        font.color = sel ? C_WHITE : C_DIM;
        font.print(px + 10, y + 2, label);

        if (equipped) {
            font.color = C_GREEN;
            font.print(px + pw - 72, y + 2, "EQUIPPED");
        } else if (hangTab === 3 && owned) {
            font.color = C_GREEN;
            font.print(px + pw - 52, y + 2, "MAX");
        } else if (owned) {
            font.color = C_CYAN;
            font.print(px + pw - 52, y + 2, "OWNED");
        } else {
            font.color = (save.credits >= price) ? C_YELLOW : C_RED;
            font.print(px + pw - 52, y + 2, `${price}c`);
        }
    }

    font.color = C_DIM;
    font.print(px + 8, CH - 22, "CROSS buy/equip  L1/R1 tab");
}

// ---------------------------------------------------------------------------
// Render helpers
// ---------------------------------------------------------------------------
function syncScene(camX, camY, camZ) {
    for (let i = 0; i < stars.length; i++) {
        const s = stars[i];
        s.obj.position = { x: s.x, y: s.y, z: s.z };
        s.obj.scale = { x: s.sc, y: s.sc, z: s.sc };
    }
    for (let i = 0; i < planets.length; i++) {
        const p = planets[i];
        p.obj.position = { x: p.x, y: p.y, z: p.z };
        p.obj.scale = { x: p.sc, y: p.sc, z: p.sc };
        p.obj.rotation = billboardToCamera(p.x, p.y, p.z, camX, camY, camZ);
    }
    for (let i = 0; i < bullets.length; i++) {
        if (bullets[i].alive) {
            bullets[i].obj.position = { x: bullets[i].x, y: bullets[i].y, z: bullets[i].z };
        }
    }
    for (let i = 0; i < enemyPool.length; i++) {
        const e = enemyPool[i];
        if (!e.active) continue;
        const o = e.type === 1 ? e.objRock : e.objFighter;
        o.position = { x: e.x, y: e.y, z: e.z };
        o.rotation = { x: e.rx, y: e.ry, z: e.rz };
        if (e.type === 1) o.scale = { x: e.sc, y: e.sc, z: e.sc };
    }
    for (let i = 0; i < fx.length; i++) {
        if (!fx[i].alive) continue;
        const prog = 1.0f - fx[i].life / fx[i].maxLife;
        const pop = Math.sin(prog * 3.141592f);
        const sc = fx[i].baseSc * (0.35f + pop * 2.4f) * (1.0f - prog * 0.35f);
        fx[i].obj.position = { x: fx[i].x, y: fx[i].y, z: fx[i].z };
        fx[i].obj.rotation = { x: prog * 0.8f, y: fx[i].spin + prog * 1.6f, z: 0.0f };
        fx[i].obj.scale = { x: sc, y: sc, z: sc };
    }
    for (let i = 0; i < particles.length; i++) {
        const p = particles[i];
        if (!p.alive) continue;
        const fade = p.life / p.maxLife;
        const ease = fade * fade;
        p.obj.position = { x: p.x, y: p.y, z: p.z };
        const s = p.sc * ease;
        const age = 1.0f - fade;

        if (p.slotKind === 0) {
            p.obj.rotation = { x: pitch, y: 0.0f, z: roll };
            p.obj.scale = { x: s * 0.7f, y: s * 0.7f, z: s * 1.8f };
        } else if (p.slotKind === 1) {
            p.obj.rotation = { x: age * 2.1f + p.spin, y: age * 3.2f, z: p.spin * 0.5f };
            p.obj.scale = { x: s * 0.35f, y: s * 0.35f, z: s * 2.8f };
        } else if (p.slotKind === 2) {
            p.obj.rotation = { x: age * 1.4f, y: age * 2.0f + p.spin, z: p.spin };
            p.obj.scale = { x: s, y: s, z: s };
        } else if (p.slotKind === 3) {
            p.obj.rotation = { x: age * 4.0f, y: age * 5.0f, z: age * 2.0f };
            p.obj.scale = { x: s * 1.2f, y: s * 1.2f, z: s * 1.2f };
        } else {
            p.obj.rotation = { x: age * 0.6f, y: age * 0.9f + p.spin, z: 0.0f };
            p.obj.scale = { x: s * 1.1f, y: s * 1.1f, z: s * 0.85f };
        }
    }
}

function renderScene(showShip) {
    for (let i = 0; i < planets.length; i++) planets[i].obj.render();
    for (let i = 0; i < stars.length; i++) stars[i].obj.render();

    for (let i = 0; i < enemyPool.length; i++) {
        const e = enemyPool[i];
        if (!e.active) continue;
        (e.type === 1 ? e.objRock : e.objFighter).render();
    }
    for (let i = 0; i < bullets.length; i++) {
        if (bullets[i].alive) bullets[i].obj.render();
    }
    for (let i = 0; i < fx.length; i++) {
        if (fx[i].alive) fx[i].obj.render();
    }
    for (let i = 0; i < particles.length; i++) {
        if (particles[i].alive) particles[i].obj.render();
    }

    if (showShip) {
        if (state === STATE_PLAY) {
            const pulse = 0.75f + Math.sin(frame * 0.6f) * 0.2f + (pad.pressed(Pads.R2) ? 0.6f : 0.0f);
            thruster.position = { x: shipX, y: shipY, z: -1.15f };
            thruster.rotation = { x: pitch, y: 0.0f, z: roll };
            thruster.scale = { x: 1.0f, y: 1.0f, z: pulse };
            thruster.render();
        }
        ship.render();
    }
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------
function drawBackdrop() {
    // Subtle vignette only — planets are 3D now.
    Draw.rect(0, 0, CW, 28, Color.new(0, 0, 0, 80));
    Draw.rect(0, CH - 28, CW, 28, Color.new(0, 0, 0, 80));
}

function drawLives() {
    for (let i = 0; i < lives; i++) {
        const x = CW - 40 - i * 26;
        const y = 26;
        Draw.triangle(x, y + 14, x + 10, y + 14, x + 5, y, C_GREEN);
    }
}

function drawBoost(boosting) {
    const bw = 120, x = 16, y = CH - 26;
    Draw.rect(x, y, bw, 10, Color.new(30, 40, 60));
    const frac = clamp((speed - BASE_SPEED * 0.4f) / (BASE_SPEED * 2.1f), 0.0f, 1.0f);
    Draw.rect(x, y, bw * frac, 10, boosting ? C_YELLOW : C_CYAN);
    font.color = C_DIM;
    font.print(x, y - 20, "SPEED");
}

function drawHud() {
    font.color = C_WHITE;
    font.print(16, 14, `SCORE ${score}`);
    font.color = C_CYAN;
    font.print(16, 34, `WAVE ${wave}`);

    if (state === STATE_MENU) {
        font.color = C_WHITE;
        font.print(CW * 0.5f - 70, 150, "S T A R   F O X");
        font.color = C_YELLOW;
        font.print(CW * 0.5f - 95, 190, "Press CROSS for Hangar");
        font.color = C_DIM;
        font.print(CW * 0.5f - 130, 230, "Left Stick: steer   CROSS/R1: fire");
        font.print(CW * 0.5f - 130, 250, "R2: boost   L2: brake   TRIANGLE: quit");
    } else if (state === STATE_HANGAR) {
        drawHangarUi();
    } else if (state === STATE_OVER) {
        font.color = C_RED;
        font.print(CW * 0.5f - 55, 140, "RUN OVER");
        font.color = C_WHITE;
        font.print(CW * 0.5f - 65, 170, `Score ${score}  Wave ${wave}`);
        font.color = C_CYAN;
        font.print(CW * 0.5f - 75, 195, `+${runEarned} credits`);
        font.color = C_DIM;
        font.print(CW * 0.5f - 55, 218, `Bank ${save.credits}`);
        font.color = C_YELLOW;
        font.print(CW * 0.5f - 85, 245, "CROSS = Hangar");
    } else {
        drawLives();
        drawBoost(pad.pressed(Pads.R2));
        font.color = C_CYAN;
        font.print(CW - 110, 34, `${save.credits}c`);
    }

    font.color = C_DIM;
    font.print(CW - 96, 14, `${Screen.getFPS()} FPS`);
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------
Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);

Camera.position(CAM_X, CAM_Y, CAM_Z);
Camera.target(LOOK_X, LOOK_Y, LOOK_Z);
Camera.update();

while (true) {
    pad.update();

    if (shake > 0.0f) shake -= 0.06f;

    if (pad.justPressed(Pads.TRIANGLE)) std.reload("main.js");

    if (state === STATE_MENU) {
        menuSpin += 0.02f;
        updateWorld(BASE_SPEED * 0.4f);
        if (pad.justPressed(Pads.CROSS) || pad.justPressed(Pads.START)) {
            state = STATE_HANGAR;
            hangTab = 0;
            hangSel = 0;
            partSlot = 0;
            rebuildShip();
        }
    } else if (state === STATE_HANGAR) {
        menuSpin += 0.02f;
        updateWorld(BASE_SPEED * 0.25f);
        updateHangar();
    } else if (state === STATE_OVER) {
        updateWorld(BASE_SPEED * 0.4f);
        if (pad.justPressed(Pads.CROSS) || pad.justPressed(Pads.START)) {
            state = STATE_HANGAR;
            hangTab = 4;
            hangSel = 0;
            rebuildShip();
        }
    } else {
        updatePlay();
    }

    // Ship transform
    if (state === STATE_PLAY) {
        ship.position = { x: shipX, y: shipY, z: 0.0f };
        ship.rotation = { x: pitch, y: 0.0f, z: roll };
    } else {
        ship.position = { x: 0.0f, y: 0.0f, z: 0.0f };
        ship.rotation = { x: 0.0f, y: menuSpin, z: 0.0f };
    }

    // Fixed chase camera — only nudge position for hit-shake (never Camera.target per frame).
    let sx = 0.0f, sy = 0.0f;
    if (shake > 0.0f) {
        sx = Math.randomf(-shake, shake) * 0.6f;
        sy = Math.randomf(-shake, shake) * 0.6f;
    }

    const camX = CAM_X + sx;
    const camY = CAM_Y + sy;
    const camZ = CAM_Z;

    syncScene(camX, camY, camZ);

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);
    Screen.clear(SPACE);
    drawBackdrop();

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
    Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);
    Render.begin();

    Camera.position(camX, camY, camZ);
    Camera.update();

    const showShip = (state !== STATE_PLAY) || (invuln <= 0) || (Math.trunc(invuln / 6) % 2 === 0);
    renderScene(showShip);

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);
    drawHud();
    Screen.flip();
}
