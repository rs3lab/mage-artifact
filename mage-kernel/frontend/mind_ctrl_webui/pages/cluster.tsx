import { GetServerSideProps } from 'next'
import Head from 'next/head'
import { ChakraProvider } from '@chakra-ui/react';
import { extendTheme } from '@chakra-ui/react';
import { Global } from '@emotion/react';
// import { Typography, Paper, Grid, Box, Container, makeStyles } from '@material-ui/core';
import { Box, Text, Flex, Container, Divider, Button, Select } from '@chakra-ui/react';
import React from 'react';
// import Redis from 'ioredis'
import { useState, useEffect } from 'react';
import { useRouter } from 'next/router';

// ClusterManagementPanel
import ClusterManagementPanel from './ClusterManagementPanel';

interface ClusterProps {
    initLogs: [ [key: string, value: string] ] | undefined | null
    runLogs: [ [key: string, value: string] ] | undefined | null
}

const theme = extendTheme({
  fonts: {
    body: 'Open Sans, sans-serif',
    heading: 'Raleway, sans-serif',
  },
});

const Cluster: React.FC<ClusterProps> = ({ initLogs, runLogs }) => {
  const [logs, setLogs] = useState({ initLogs, runLogs });
  const router = useRouter();
  const { cluster } = router.query;

  // == Logs part ==
  // The useEffect hook will run after every render
  useEffect(() => {
    // Define a function that fetches the logs from the server
    const fetchLogs = async () => {
      // console.log('Fetching logs');
      if (cluster !== undefined) {
        const res = await fetch(`http://172.28.229.152:3000/api/cluster/${cluster}`);
        const newLogs = await res.json();
        setLogs(newLogs);
        return;
      }
    };

    // Call fetchLogs initially
    fetchLogs();

    // Then set up the interval to call fetchLogs every 5 seconds
    const intervalId = setInterval(fetchLogs, 3000);

    // Return a cleanup function to clear the interval when the component is unmounted
    return () => clearInterval(intervalId);
  }, [cluster]);  // Empty array means this effect runs once when the component is mounted

  // console.log(logs.initLogs);
  return (
    <ChakraProvider theme={theme}>
      <Global
        styles={`
          body {
            font-family: 'Open Sans', sans-serif;
          }
        `}
      />
      <Head>
        <title>Cluster Logs</title>
      </Head>
      <Container maxW="container.xl" p={4}>
        <Box my={4}>
          {/* <Text fontSize="3xl" mb={4}> */}
          <Text as={'span'} color={'green.700'} fontSize="3xl" mb={4}>
            Logs for Cluster {cluster}
          </Text>

          <Flex direction={{ base: 'column', md: 'row' }} gap={4}>
            <Box flex={1} borderWidth={1} borderRadius="lg" p={4} mb={4}>
              <Text fontSize="2xl" mb={2}>
                Init Logs
              </Text>
              <Divider mb={2} />
              {logs.initLogs &&
                logs.initLogs.map((log) => (
                  <Box key={log[0]}>
                    <Text>
                      <strong>{log[0]}</strong>: {log[1]}
                    </Text>
                  </Box>
                ))}
            </Box>

            <Box flex={1} borderWidth={1} borderRadius="lg" p={4} mb={4}>
              <Text fontSize="2xl" mb={2}>
                Run Logs
              </Text>
              <Divider mb={2} />
              {logs.runLogs &&
                logs.runLogs.map((log) => (
                  <Box key={log[0]}>
                    <Text>
                      <strong>{log[0]}</strong>: {log[1]}
                    </Text>
                  </Box>
                ))}
            </Box>
          </Flex>
        </Box>
        <ClusterManagementPanel />
      </Container>
    </ChakraProvider>
  );
}

export default Cluster
