import { useState } from 'react';
import { Divider, Button, Select, Box, Flex, Text } from '@chakra-ui/react';

function handleFileUpload(event: React.ChangeEvent<HTMLInputElement>) {
  const file = event.target.files[0];
  const reader = new FileReader();
  reader.onload = (event) => {
    const content = event.target.result.toString();
    const json = JSON.parse(content);
    // Do something with the JSON data
  };
  reader.readAsText(file);
}

function ClusterManagementPanel() {

  // == Script part ==
  const [scriptOutput, setScriptOutput] = useState('//Script output will appear here');
  const scriptHandlerPath = '/api/run_script/';
  const fileUploadPath = '/api/upload';

  // script selection menu
  // Define an array of scripts to choose from
  const scripts = [
    { value: 'script1', label: 'Script 1' },
    { value: 'script2', label: 'Script 2' },
    { value: 'script3', label: 'Script 3' },
  ];

  // Define a state variable to keep track of the selected script
  const [selectedScript, setSelectedScript] = useState('');

  // Define a function to handle the script selection
  const handleScriptSelect = (event: React.ChangeEvent<HTMLSelectElement>) => {
    setSelectedScript(event.target.value);
  };

  // log refresher
  const fetchLog = async () => {
    const response = await fetch(scriptHandlerPath + `${selectedScript}` + `?action=log`);
    const text = await response.text();
    setScriptOutput(text);
  };

  // Run script when a button is clicked
  const handleClick = async () => {
    try {
      setInterval(fetchLog, 1000);
      const response = await fetch(scriptHandlerPath + `${selectedScript}`);
      const data = await response.json();
      console.log(data.scriptNameStr);
      setScriptOutput(data.output);
    } catch (error) {
      console.error('Error executing script:', error);
    }
  };

  // Upload container JSON
  function handleFileUpload(file: File) {
    const reader = new FileReader();
    reader.onload = (event) => {
      const content = event.target.result.toString();
      const json = JSON.parse(content);
      // TODO: Do something with the JSON data
    };
    reader.readAsText(file);
  }

  // == Upload part ==
  // Define a state variable to keep track of the upload progress
  const [progress, setProgress] = useState(0);
  // Define a state variable to keep track of whether the user is dragging a file over the dropzone
  const [dragging, setDragging] = useState(false);

  // Define functions to handle the file upload (drag and drop)
  function handleDragEnter(event: React.DragEvent<HTMLDivElement>) {
    event.preventDefault();
    setDragging(true);
  }

  function handleDragLeave(event: React.DragEvent<HTMLDivElement>) {
    event.preventDefault();
    setDragging(false);
  }

  function uploadFile(file: File) {
    const formData = new FormData();
    formData.append('file', file);
  
    const xhr = new XMLHttpRequest();
    xhr.open('POST', fileUploadPath, true);
  
    // Track upload progress
    xhr.upload.onprogress = (event) => {
      if (event.lengthComputable) {
        const percentComplete = (event.loaded / event.total) * 100;
        console.log(`Upload progress: ${percentComplete}%`);
        setProgress(percentComplete);
      }
    };
    xhr.setRequestHeader('Content-Type', file.type);
    xhr.send(file);
    console.log(file);
    handleFileUpload(file);
  }
  
  function handleDrop(event: React.DragEvent<HTMLDivElement>) {
    event.preventDefault();
    setDragging(false);
    const file = event.dataTransfer.files[0];
    uploadFile(file);
  }
  
  function handleButtonClick() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    input.onchange = (event) => {
      const file = (event.target as HTMLInputElement).files[0];
      uploadFile(file);
    };
    input.click();
  }

  return (
    <Box>
      <Text fontSize="3xl" mb={2}>
        Cluster management panel (WIP 🧪)
      </Text>
      <Divider mb={10} />
      <Flex alignItems="center">
        <Select value={selectedScript} onChange={handleScriptSelect} mr={2}>
          <option value="">Select a script</option>
          {scripts.map((script) => (
            <option key={script.value} value={script.value}>{script.label}</option>
          ))}
        </Select>
        <Button onClick={handleClick} colorScheme="blue">Run Script</Button>
      </Flex>
      <Box overflowY="scroll" minHeight="400px" maxHeight="400px" minWidth="800px" bgColor="rgba(135, 206, 250, 0.15)">
        <pre>{scriptOutput}</pre>
      </Box>
      <Divider mb={2} />
      <Flex flexDirection="column" style={{ width: '100%' }}>
        <Flex alignItems="center">
          <div
              style={{ border: `2px dashed ${dragging ? 'blue' : 'gray'}`, padding: '1rem', width: '100%' }}
              onDragEnter={handleDragEnter}
              onDragOver={handleDragEnter}
              onDragLeave={handleDragLeave}
              onDrop={handleDrop}
            >
            Drag and drop a JSON file here
          </div>
          &nbsp;&nbsp;&nbsp;OR&nbsp;&nbsp;&nbsp;
          <Button mt={2} onClick={handleButtonClick} colorScheme="blue">
            Upload JSON file
          </Button>
        </Flex>
        <progress value={progress} max="100" style={{ width: '100%' }}/>
      </Flex>
    </Box>
  );
}

export default ClusterManagementPanel;