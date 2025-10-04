const { Config } = require('./dist/Config');

console.log('Testing configuration loading...');

try {
    const config = Config.findAndLoadConfig(['--config', 'test-config.json']);
    console.log('Configuration loaded successfully!');
    console.log('Instance name:', config.get('instance.name'));
    console.log('Instance role:', config.get('instance.role'));
    console.log('Network port:', config.get('network.port'));
    console.log('Logging level:', config.get('logging.level'));
    console.log('Full config:', JSON.stringify(config.getAll(), null, 2));
} catch (err) {
    console.error('Configuration loading failed:', err);
}