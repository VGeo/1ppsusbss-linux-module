/*
 * Host side of program that tests usb speed of b5 cards
 * Copyright (C) 2007 Daniel Drake <dsd@gentoo.org>
 * Copyright (C) 2010 Sergey Kolemagin <s.kolemagin@metrotek.spb.ru>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <libusb.h>

#define ENDPOINT1 0x01
#define ENDPOINT2 0x81
#define ENDPOINT3 0x3
#define RXBUFLEN  1600

#define TIMEOUT 300

typedef struct device_ids {
  uint16_t idVendor;
  uint16_t idProduct;
} device_ids_t;

static libusb_device * find_dev(libusb_device **devs, device_ids_t * ids )
{
  libusb_device *dev;
  int i = 0;

  while ((dev = devs[i++]) != NULL) {
    struct libusb_device_descriptor desc;
    int r = libusb_get_device_descriptor(dev, &desc);
    if (r < 0) {
      printf( "Failed to get device descriptor" );
      return NULL;
    }

    if( ( desc.idVendor == ids->idVendor ) &&
        ( desc.idProduct == ids->idProduct ) ) {
          printf( "Device has been detected: %04x:%04x (bus %d, device %d)\n",
          desc.idVendor, desc.idProduct,
          libusb_get_bus_number(dev), libusb_get_device_address(dev));

      return dev;
    }
  }

  return NULL;
}

/* 
 * Finds USB device with specified IDs
 * */
static int stm_find_device( libusb_device *** devs, 
                                  device_ids_t * ids,
                                  libusb_device ** stm_dev )
{
  ssize_t cnt;
  cnt = libusb_get_device_list(NULL, devs);
  if (cnt < 0) {
    return -1;
  }

  *stm_dev = find_dev( *devs, ids );
  
  if( stm_dev == NULL ) {
    puts("Device hasn't been detected");
    return -1;
  }

  return 0;
}

/*
 * Gets USB device handle
 */
static int stm_open_device( libusb_device * stm_dev,
                                  libusb_device_handle ** stm_handle,
                                  int interface_num )
{
  int r;
  r = libusb_open( stm_dev, stm_handle ); 
  if ( r != 0 ) {
    puts("Open failed");
    return -1;
  } 

  r = libusb_claim_interface( *stm_handle, interface_num );
  if ( r != 0 ) {
    printf( "Can't claim interface %d: reason %s (%d)\n", interface_num, libusb_strerror(r), r);
    return -1;
  } 

  return 0;
}

                  
int main( int argc, char** argv ) 
{
  libusb_device **devs;
  device_ids_t stm_ids;
  libusb_device * stm_dev;
  libusb_device_handle * stm_handle = NULL;

  int completed = 0;
  unsigned char *rxbuf;

  int r = 0;


  stm_ids.idVendor = 0x04b4;
  stm_ids.idProduct = 0x00f0;

  r = libusb_init(NULL);
  if (r < 0) {
    puts("Init failed");
  }

  r = stm_find_device( &devs, &stm_ids, &stm_dev );
  if( r < 0 ) {
    goto find_failed;
  } 

  if( stm_dev == NULL ) {
    puts("Device hasn't been detected");
    goto find_failed;
  }

  r = stm_open_device( stm_dev, &stm_handle, 0x0 );
  if( r != 0 ) {
    goto open_failed;
  }

  rxbuf = malloc( RXBUFLEN );
  if( rxbuf == NULL ) {
    puts("Nomem");
    goto test_failed;
  }

  r = libusb_get_max_packet_size( stm_dev, ENDPOINT2 );
  if( r < 0 ) {
    printf( "Max packet size failed: reason %s (%d)\n", libusb_strerror(r), r );
    goto test_failed;
  } else {
    printf( "Max packet size %d bytes\n", r );
  }

#define BUFLEN 1
  unsigned char cmdbuf[BUFLEN];

  for( int i = 0; i < BUFLEN; ++i )
  {
    cmdbuf[i] = 0;
  }

  while(1)
  {
    cmdbuf[0] = 0;
    /* Set LED off. */
    r = libusb_bulk_transfer( stm_handle, ENDPOINT1, (unsigned char*) cmdbuf,
                                BUFLEN, &completed, TIMEOUT );
    if ( r < 0 ) {
      printf( "\nUnable to send the request command: %s (%d)\n", libusb_strerror(r), r );
      goto test_failed;
    } 
    usleep(900000);

    cmdbuf[0] = 1;
    /* Set LED on. */
    r = libusb_bulk_transfer( stm_handle, ENDPOINT1, (unsigned char*) cmdbuf,
                                BUFLEN, &completed, TIMEOUT );
    if ( r < 0 ) {
      printf( "\nUnable to send the request command: %s (%d)\n", libusb_strerror(r), r );
      goto test_failed;
    } 
    usleep(100000);

    /* Receive data from endpoint 2 to rxbuf */
    r = libusb_bulk_transfer( stm_handle, ENDPOINT2, rxbuf, 
                                 RXBUFLEN, &completed, 1 );
    if( r < 0 ) {
      //No new packet
      //printf( "\nBulk RX transfer failed: reason %s (%d)\n", libusb_strerror(r), r );
    } else {
      printf("Recv: 0x%x\n", rxbuf[0]);
    }
  }

  /* Print received data */
  for( int i = 0; i < completed; ++i )
  {
    printf( "[%d] = %u ", i, rxbuf[i] );
  }
  printf( "\n" );
        
  free( rxbuf );

  /* Error handling */
test_failed:
open_failed:
  if ( NULL != stm_handle ) {
    libusb_close( stm_handle );
  }

find_failed:
  libusb_free_device_list(devs, 1);
  libusb_exit( NULL );

  if ( r != 0 ) {
    printf( "Errors occured!\n" );
  } else {
    printf( "Done\n" );
  }
  return r;
}
