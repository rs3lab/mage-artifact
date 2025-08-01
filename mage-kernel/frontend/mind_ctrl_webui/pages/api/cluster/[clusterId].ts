import { NextApiRequest, NextApiResponse } from 'next';
import Redis from 'ioredis';

// Create a new Redis client
const client = new Redis({
    host: 'localhost',  // replace with your Redis server IP
    port: 6379  // replace with your Redis server port
});

// API route handler
export default async function handler(req: NextApiRequest, res: NextApiResponse) {
    const {
        query: { clusterId },
    } = req;

    // console.log(`Connecting to Redis... ${clusterId}`);

    // Fetch logs from the Redis server
    try {
        const keys = await client.keys(`cluster${clusterId}::*`);
        const multi = client.multi();
        keys.forEach(key => multi.get(key));
        const replies = await multi.exec();

        // Format the logs into the required structure
        const logs = keys.reduce((acc, key, index) => {
            const parts = key.split('::');
            if (parts.length !== 3) {
                console.warn(`Unexpected key format: ${key}`);
                return acc;
            }

            const [_, keyClass, keyItem] = parts;
            acc[keyClass] = { ...(acc[keyClass] || {}), [keyItem]: replies[index][1] };
            return acc;
        }, { init: {}, run: {} });
        // Convert the objects to arrays and sort them
        const sortedInitLogs = Object.entries(logs.init).sort((a, b) => (a[1] as string).localeCompare(b[1] as string));
        const sortedRunLogs = Object.entries(logs.run).sort((a, b) => (a[1] as string).localeCompare(b[1] as string));
        // console.log(sortedInitLogs);
        // const sortedInitLogs = Object.entries(logs.init)
        //     .sort((a, b) => (a[1] as string).localeCompare(b[1] as string))
        //     .map(([key, value]) => ({ key, message: value }));

        // const sortedRunLogs = Object.entries(logs.run)
        //     .sort((a, b) => (a[1] as string).localeCompare(b[1] as string))
        //     .map(([key, value]) => ({ key, message: value }));

        // Send the sorted logs as a JSON response
        res.send({ initLogs: sortedInitLogs, runLogs: sortedRunLogs });
    } catch (error) {
        console.error(error);
        res.status(500).json({ error: 'Error fetching data from Redis' });
    }
}
