import { speciesData } from './docs/speciesData.js';
import { moveData } from './docs/moves.js';
import fs from 'fs';

let badgeCount = 8;
let levelCap = 100;

function loadTMs() {
  const file = fs.readFileSync('./docs/Tm-locatoins.txt', 'utf8');
  const lines = file.split('\n');
  const tmMap = {};
  const tmBadges = {};

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

const typeChart = {
  Normal: { weakTo: ['Fighting'], resistances: [], immuneTo: ['Ghost'] },
  Fire: { weakTo: ['Water', 'Rock', 'Ground'], resistances: ['Fire', 'Grass', 'Ice', 'Bug', 'Steel', 'Fairy'], strongAgainst: ['Grass', 'Ice', 'Bug', 'Steel'] },
  Water: { weakTo: ['Electric', 'Grass'], resistances: ['Fire', 'Water', 'Ice', 'Steel'], strongAgainst: ['Fire', 'Ground', 'Rock'] },
  Grass: { weakTo: ['Fire', 'Flying', 'Bug', 'Ice'], resistances: ['Water', 'Grass', 'Electric', 'Ground'], strongAgainst: ['Water', 'Ground', 'Rock'] },
  Electric: { weakTo: ['Ground'], resistances: ['Electric', 'Flying', 'Steel'], strongAgainst: ['Water', 'Flying'] },
  Ground: { weakTo: ['Water', 'Ice', 'Grass'], resistances: ['Poison', 'Rock'], immuneTo: ['Electric'], strongAgainst: ['Fire', 'Electric', 'Poison', 'Rock', 'Steel'] },
  Flying: { weakTo: ['Electric', 'Rock', 'Ice'], resistances: ['Grass', 'Fighting', 'Bug'], immuneTo: ['Ground'], strongAgainst: ['Grass', 'Fighting', 'Bug'] },
  Rock: { weakTo: ['Water', 'Grass', 'Fighting', 'Steel', 'Ground'], resistances: ['Normal', 'Fire', 'Flying', 'Poison'], strongAgainst: ['Fire', 'Ice', 'Flying', 'Bug'] },
};

function getLegalMoves(pokemonName) {
  const mon = speciesData[pokemonName.toUpperCase()];
  if (!mon) throw new Error(`❌ Pokémon "${pokemonName}" not found.`);

  const levelup = [];
  const tmhm = [];
  const egg = [];
  const tutor = [];

  for (const [move, level] of mon.levelUpLearnsets || []) {
    if (level <= levelCap) levelup.push({ move, level });
  }

  for (const tmMove of mon.TMHMLearnsets || []) {
    const badgeReq = Object.entries(tmMap).find(([, name]) => name === tmMove);
    const tmId = badgeReq?.[0];
    const requiredBadge = tmId ? tmBadges[tmId] : null;
    if (!requiredBadge || requiredBadge <= badgeCount) tmhm.push(tmMove);
  }

  for (const move of mon.eggMovesLearnsets || []) egg.push(move);
  for (const move of mon.tutorLearnsets || []) tutor.push(move);

  const all = [
    ...new Set([
      ...levelup.map(obj => obj.move),
      ...tmhm,
      ...egg,
      ...tutor
    ])
  ];

  return { levelup, tmhm, egg, tutor, all };
}

function getTrainerTeam(trainerName) {
  const raw = JSON.parse(fs.readFileSync('./docs/Trainer-battles.json', 'utf8'));

  const trainer = Object.values(raw).find(t => t.Trainer.toLowerCase() === trainerName.toLowerCase());
  if (!trainer) throw new Error(`❌ Trainer "${trainerName}" not found.`);

  return trainer.Team.map(p => ({
    name: p.Name.toUpperCase(),
    level: parseInt(p.Level),
    ability: p.Ability,
    moves: p.Moves,
    item: p.Item,
    nature: p.Nature,
    ivs: p.IVs
  }));
}

function buildCounterTeam(trainerName, options = {}) {
  const trainerTeam = getTrainerTeam(trainerName);
  const allMons = Object.values(speciesData);
  const counterTeam = [];

  for (const mon of allMons) {
    if (counterTeam.length >= 6) break;
    if (options.monotype && !mon.types.includes(options.monotype)) continue;

    const { all: legalMoves } = getLegalMoves(mon.name);
    if (!legalMoves || legalMoves.length === 0) continue;

    const stabTypes = new Set(mon.types);
    let goodCounter = false;

    for (const foe of trainerTeam) {
      const foeData = speciesData[foe.name.toUpperCase()];
      if (!foeData) continue;

      const weaknesses = foeData.types?.flatMap(t => typeChart[t]?.weakTo || []) || [];

      const stabCoverage = legalMoves.some(move => {
        const m = moveData[move];
        return m && stabTypes.has(m.type) && weaknesses.includes(m.type);
      });

      if (stabCoverage) {
        goodCounter = true;
        break;
      }
    }

    if (goodCounter) {
      counterTeam.push({
        name: mon.name,
        types: mon.types,
        speed: mon.baseStats?.speed || 0,
        moves: legalMoves.slice(0, 4)
      });
    }
  }

  return counterTeam.sort((a, b) => b.speed - a.speed);
}

function setBadgeCount(n) { badgeCount = n; }
function setLevelCap(n) { levelCap = n; }

function handleQuery(query) {
  const lc = query.toLowerCase();

  if (lc.startsWith('badge')) {
    const num = parseInt(query.split(' ')[1]);
    setBadgeCount(num);
    return `✔️ Badge count set to ${num}`;
  }

  if (lc.startsWith('level cap')) {
    const num = parseInt(query.split(' ')[2]);
    setLevelCap(num);
    return `✔️ Level cap set to ${num}`;
  }

  const moveMatch = query.match(/(level up|tmhm|egg|tutor) moveset for (.+)/i);
  if (moveMatch) {
    const section = moveMatch[1].toLowerCase().replace(' ', '');
    const monName = moveMatch[2].trim();
    const legal = getLegalMoves(monName);
    return legal[section] || `❌ No ${section} moves for ${monName}`;
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

  return `❌ Unknown query. Try:\n- level up moveset for Garchomp\n- counterteam for Roxanne using only monotype Grass`;
}

export {
  getLegalMoves,
  buildCounterTeam,
  getTrainerTeam,
  setBadgeCount,
  setLevelCap,
  handleQuery
};
