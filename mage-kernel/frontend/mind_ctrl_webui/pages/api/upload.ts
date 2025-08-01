// pages/api/upload.ts

import { NextApiRequest, NextApiResponse } from 'next';
import multer from 'multer';
import fs from 'fs-extra';
import path from 'path';

const jsonUploadPath: string = 'json_uploads/tmp.json';
const upload = multer({ storage: multer.memoryStorage(), preservePath: true });

export default async function handler(req: NextApiRequest, res: NextApiResponse) {
  if (req.method !== 'POST') {
    res.status(405).json({ error: 'Method Not Allowed' });
    return;
  }

  try {
    await upload.single('file')(req, res, () => {});
    console.log(req);
    const jsonData = JSON.stringify(req.body, null, 2);
    await fs.writeFile(jsonUploadPath, jsonData);
    res.status(200).json({ message: 'File uploaded successfully' });

  } catch (error) {
    console.error('Error uploading file:', error);
    res.status(500).json({ error: 'File upload failed' });
  }
}

// const upload = multer({ dest: jsonUploadPath });
// export default async function handler(req: NextApiRequest, res: NextApiResponse) {
//   if (req.method !== 'POST') {
//     res.status(405).json({ error: 'Method Not Allowed' });
//     return;
//   }

//   try {
//     await upload.single('file')(req, res, async (error) => {
//       if (error) {
//         console.error('Error uploading file:', error);
//         res.status(500).json({ error: 'File upload failed' });
//         return;
//       }

//     //   if (!(req.file as Express.Multer.File)) {
//     //     res.status(400).json({ error: 'No file uploaded' });
//     //     return;
//     //   }

//     //   const { path: tempPath } = req.file as Express.Multer.File;
//     //   const targetPath = path.join('json_uploads/', req.file.originalname);

//       try {
//         console.log(req);
//         // const jsonData = JSON.stringify(req.body);
//         // await fs.writeFile(jsonUploadPath, jsonData);
//         // await fs.remove(tempPath);
//         res.status(200).json({ message: 'File uploaded successfully' });
//       } catch (error) {
//         console.error('Error writing file:', error);
//         res.status(500).json({ error: 'File upload failed' });
//       }
//     });
//   } catch (error) {
//     console.error('Error uploading file:', error);
//     res.status(500).json({ error: 'File upload failed' });
//   }
// }