// engine.js
import { speciesData } from './docs/speciesData.js';
import { moveData } from './docs/moves.js';
import fs from 'fs';

// === GLOBAL CONSTRAINTS ===
let badgeCount = 8;
let levelCap = 100;

// === LOAD TM NAME + BADGE MAPPING ===
function loadTMs() {
  const file = fs.readFileSync('./docs/Tm-locatoins.txt', 'utf8');
  const lines = file.split('\n');
  const tmMap = {}; // TM01 → Move
  const tmBadges = {}; // TM01 → badge 4

  for (let line of lines) {
    const match = line.match(/(TM\d+): (.+?) \(Requires badge (\d+)\)/);
    if (match) {
      const [, tmId, moveName, badge] = match;
      tmMap[tmId] = moveName.trim();
      tmBadges[tmId] = parseInt(badge);
    }
  }

  return { tmMap, tmBadges };
}

const { tmMap, tmBadges } = loadTMs();

// === TYPE CHART ===
const typeChart = {
  Normal: { weakTo: ['Fighting'], resistances: [], immuneTo: ['Ghost'] },
  Fire: { weakTo: ['Water', 'Rock', 'Ground'], resistances: ['Fire', 'Grass', 'Ice', 'Bug', 'Steel', 'Fairy'], strongAgainst: ['Grass', 'Ice', 'Bug', 'Steel'] },
  Water: { weakTo: ['Electric', 'Grass'], resistances: ['Fire', 'Water', 'Ice', 'Steel'], strongAgainst: ['Fire', 'Ground', 'Rock'] },
  Grass: { weakTo: ['Fire', 'Flying', 'Bug', 'Ice'], resistances: ['Water', 'Grass', 'Electric', 'Ground'], strongAgainst: ['Water', 'Ground', 'Rock'] },
  Electric: { weakTo: ['Ground'], resistances: ['Electric', 'Flying', 'Steel'], strongAgainst: ['Water', 'Flying'] },
  Ground: { weakTo: ['Water', 'Ice', 'Grass'], resistances: ['Poison', 'Rock'], immuneTo: ['Electric'], strongAgainst: ['Fire', 'Electric', 'Poison', 'Rock', 'Steel'] },
  Flying: { weakTo: ['Electric', 'Rock', 'Ice'], resistances: ['Grass', 'Fighting', 'Bug'], immuneTo: ['Ground'], strongAgainst: ['Grass', 'Fighting', 'Bug'] },
  Rock: { weakTo: ['Water', 'Grass', 'Fighting', 'Steel', 'Ground'], resistances: ['Normal', 'Fire', 'Flying', 'Poison'], strongAgainst: ['Fire', 'Ice', 'Flying', 'Bug'] },
  // Add more as needed...
};

// === GET LEGAL MOVES ===
function getLegalMoves(pokemonName) {
  const mon = speciesData[pokemonName.toUpperCase()];
  if (!mon) throw new Error(`Pokémon ${pokemonName} not found.`);

  const legalMoves = new Set();

  for (const [lvl, moves] of Object.entries(mon.levelUpMoves)) {
    if (parseInt(lvl) <= levelCap) {
      moves.forEach(m => legalMoves.add(m));
    }
  }

  for (const tm of mon.tmMoves || []) {
    const badgeReq = tmBadges[tm];
    const moveName = tmMap[tm];
    if (badgeReq && badgeReq <= badgeCount && moveName) {
      legalMoves.add(moveName);
    }
  }

  return Array.from(legalMoves);
}

// === PARSE TRAINER TEAMS ===
function getTrainerTeam(trainerName) {
  const raw = fs.readFileSync('./docs/trainer-battles.txt', 'utf8');
  const lines = raw.split('\n');
  const team = [];
  let currentTrainer = null;

  for (let line of lines) {
    if (line.trim().endsWith(':')) {
      currentTrainer = line.trim().replace(':', '');
    } else if (currentTrainer === trainerName) {
      const match = line.match(/- (.+), level (\d+), Ability: (\w+)/);
      if (match) {
        const [, name, level, ability] = match;
        team.push({ name: name.trim(), level: parseInt(level), ability });
      }
    }
  }

  if (team.length === 0) throw new Error(`Trainer ${trainerName} not found.`);
  return team;
}

// === COUNTER LOGIC ===
function buildCounterTeam(trainerName, options = {}) {
  const trainerTeam = getTrainerTeam(trainerName);
  const allMons = Object.values(speciesData);
  const counterTeam = [];

  for (const mon of allMons) {
    if (counterTeam.length >= 6) break;
    if (options.monotype && !mon.types.includes(options.monotype)) continue;

    const legalMoves = getLegalMoves(mon.name);
    if (legalMoves.length === 0) continue;

    const stabTypes = new Set(mon.types);
    let isEffective = false;

    for (const foe of trainerTeam) {
      const foeData = speciesData[foe.name.toUpperCase()];
      if (!foeData) continue;

      const weaknesses = foeData.types.flatMap(t => typeChart[t]?.weakTo || []);
      const resistances = foeData.types.flatMap(t => typeChart[t]?.resistances || []);

      const stabCoverage = legalMoves.some(move => {
        const m = moveData[move];
        return m && stabTypes.has(m.type) && weaknesses.includes(m.type);
      });

      if (stabCoverage) {
        isEffective = true;
        break;
      }
    }

    if (isEffective) {
      counterTeam.push({
        name: mon.name,
        types: mon.types,
        speed: mon.baseStats?.speed || 0,
        moves: legalMoves.slice(0, 4),
      });
    }
  }

  return counterTeam.sort((a, b) => b.speed - a.speed);
}

// === SETTERS ===
function setBadgeCount(n) {
  badgeCount = n;
}
function setLevelCap(n) {
  levelCap = n;
}

// === GPT ROUTER ===
function handleQuery(query) {
  const lc = query.toLowerCase();

  if (lc.startsWith('badge')) {
    const num = parseInt(query.split(' ')[1]);
    setBadgeCount(num);
    return `Badge count set to ${num}`;
  }

  if (lc.startsWith('level cap')) {
    const num = parseInt(query.split(' ')[2]);
    setLevelCap(num);
    return `Level cap set to ${num}`;
  }

  if (lc.startsWith('moveset for')) {
    const name = query.split('for ')[1].trim();
    return getLegalMoves(name);
  }

  if (lc.startsWith('counterteam for')) {
    const match = query.match(/counterteam for (.+?)( using only monotype (\w+))?$/i);
    const trainer = match[1].trim();
    const mono = match[3] ? match[3].trim() : null;
    return buildCounterTeam(trainer, mono ? { monotype: mono } : {});
  }

  return "Unrecognized query.";
}

// === EXPORTS ===
export {
  getLegalMoves,
  buildCounterTeam,
  getTrainerTeam,
  setBadgeCount,
  setLevelCap,
  handleQuery
};
