// pages/api/run-script.ts

import { NextApiRequest, NextApiResponse } from 'next';
import { exec } from 'child_process';
import fs from 'fs';

export default function handler(req: NextApiRequest, res: NextApiResponse) {
  const {
    query: { scriptName },
  } = req;
  const scriptLogPath = 'script.log';

  if (req.method === 'GET' && req.query.action === 'log') {
    try {
      const log_text = fs.readFileSync(scriptLogPath, 'utf8');
      res.status(200).send(log_text);
    } catch (err) {
      console.error('Error reading log file:', err);
      res.status(500).json({ error: 'Error reading log file' });
    }
    return;
  }

  if (req.method !== 'GET') {
    res.status(405).json({ error: 'Method Not Allowed' });
    return;
  }
  // console.log(`Script name: ${scriptName}`)
  const scriptNameStr = `Script name: ${scriptName}`;
  const scriptPath = './dummy.py';
  const process = exec(`python3 ${scriptPath}`, (error, stdout) => {
    // const process = exec(`pwd`, (error, stdout) => {
    if (error) {
      console.error('Error executing script:', error);
      res.status(500).json({ error: 'Script execution failed' });
      return;
    }

    const output = stdout.trim();
    res.status(200).json({ scriptNameStr, output });
  });

  // logging
  const stream = fs.createWriteStream(scriptLogPath);
  stream.write(''); // reset the file
  process.stdout?.on('data', (data) => {
    stream.write(data);
  });
}
