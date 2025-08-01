import React from 'react';
import { ChakraProvider, ColorModeProvider, CSSReset, extendTheme } from "@chakra-ui/react";

// Customize the theme
const theme = extendTheme({
  colors: {
    brand: {
      900: "#1a365d",
      800: "#153e75",
      700: "#2a69ac",
    },
  },
  fonts: {
    heading: "Open Sans",
    body: "Raleway",
  }
});

function MindWebui({ Component, pageProps }) {
  return (
    <ChakraProvider theme={theme}>
      <ColorModeProvider
        options={{
          useSystemColorMode: true,
        }}
      >
        <CSSReset />
        <Component {...pageProps} />
      </ColorModeProvider>
    </ChakraProvider>
  );
}

export default MindWebui;
