// cli.js
import readline from 'readline';
import {
  handleQuery,
  setBadgeCount,
  setLevelCap,
  getLegalMoves,
  buildCounterTeam,
  getTrainerTeam
} from './engine.js';

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
  prompt: 'PK-Engine> '
});

console.log('\n🎮 Welcome to the PK-Engine CLI');
console.log('Type GPT-style queries like:');
console.log('   badge 4');
console.log('   level cap 50');
console.log('   moveset for Garchomp');
console.log('   counterteam for Erika using only monotype Ground\n');
rl.prompt();

rl.on('line', (line) => {
  const input = line.trim();
  if (input === 'exit' || input === 'quit') {
    rl.close();
    return;
  }

  try {
    const result = handleQuery(input);
    if (typeof result === 'string') {
      console.log(result);
    } else {
      console.dir(result, { depth: null });
    }
  } catch (err) {
    console.error('❌ Error:', err.message);
  }

  rl.prompt();
}).on('close', () => {
  console.log('👋 Goodbye!');
  process.exit(0);
});
