/*-----------------------------------------*\
|  i2c_smbus_nuvoton_nct6793d.cpp           |
|                                           |
|  Nuvoton NCT6793D SMBUS driver for Windows|
|                                           |
|  Adam Honse (CalcProgrammer1) 5/19/2019   |
\*-----------------------------------------*/

#include "i2c_smbus_nuvoton_nct6793d.h"
#include <Windows.h>
#include "inpout32.h"

#pragma comment(lib, "inpout32.lib")

s32 i2c_smbus_nuvoton_nct6793d::nct6793d_access(u16 addr, char read_write, u8 command, int size, i2c_smbus_data *data)
{
    int i, len, status, cnt;

    Out32(SMBHSTCTL, NCT6793D_SOFT_RESET);

    switch (size)
    {
    case I2C_SMBUS_QUICK:
        Out32(SMBHSTADD, (addr << 1) | read_write);
        break;
	case I2C_SMBUS_BYTE:
    case I2C_SMBUS_BYTE_DATA:
        Out32(SMBHSTADD, (addr << 1) | read_write);
        Out32(SMBHSTIDX, command);
        if (read_write == I2C_SMBUS_WRITE)
        {
            Out32(SMBHSTDAT, data->byte);
            Out32(SMBHSTCMD, NCT6793D_WRITE_BYTE);
        }
        else
        {
            Out32(SMBHSTCMD, NCT6793D_READ_BYTE);
        }
        break;
    case I2C_SMBUS_WORD_DATA:
        Out32(SMBHSTADD, (addr << 1) | read_write);
        Out32(SMBHSTIDX, command);
        if (read_write == I2C_SMBUS_WRITE)
        {
			Out32(SMBHSTDAT, data->word & 0xFF);
			Out32(SMBHSTDAT, (data->word & 0xFF00) >> 8);
            Out32(SMBHSTCMD, NCT6793D_WRITE_WORD);
        }
        else
        {
            Out32(SMBHSTCMD, NCT6793D_READ_WORD);
        }
        break;
    case I2C_SMBUS_BLOCK_DATA:
        Out32(SMBHSTADD, (addr << 1) | read_write);
        Out32(SMBHSTIDX, command);
        if (read_write == I2C_SMBUS_WRITE)
        {
            len = data->block[0];
            if (len == 0 || len > I2C_SMBUS_BLOCK_MAX)
            {
                return -EINVAL;
            }
            Out32(SMBBLKSZ, len);

			//Load 4 bytes into FIFO
			cnt = 1;
			if (len >= 4)
			{
				for (i = cnt; i <= 4; i++)
				{
					Out32(SMBHSTDAT, data->block[i]);
				}

				len -= 4;
				cnt += 4;
			}
			else
			{
				for (i = cnt; i <= len; i++)
				{
					Out32(SMBHSTDAT, data->block[i]);
				}

				len = 0;
			}
            
            Out32(SMBHSTCMD, NCT6793D_WRITE_BLOCK);
        }
        else
        {
            Out32(SMBHSTCMD, NCT6793D_READ_BLOCK);
        }
        break;
    default:
        return -EOPNOTSUPP;
    }

    Out32(SMBHSTCTL, NCT6793D_MANUAL_START);

	while ((size == I2C_SMBUS_BLOCK_DATA) && (len > 0))
	{
		if (read_write == I2C_SMBUS_WRITE)
		{
			while ((Inp32(SMBHSTSTS) & NCT6793D_FIFO_EMPTY) == 0);

			//Load more bytes into FIFO
			if (len >= 4)
			{
				for (i = cnt; i <= 4; i++)
				{
					Out32(SMBHSTDAT, data->block[i]);
				}

				len -= 4;
				cnt += 4;
			}
			else
			{
				for (i = cnt; i <= len; i++)
				{
					Out32(SMBHSTDAT, data->block[i]);
				}

				len = 0;
			}
		}
	}

	//wait for manual mode to complete
	while ((Inp32(SMBHSTSTS) & NCT6793D_MANUAL_ACTIVE) != 0);

	if ((Inp32(SMBHSTERR) & NCT6793D_NO_ACK) != 0)
	{
		return -EPROTO;
	}
	else if ((read_write == I2C_SMBUS_WRITE) || (size == I2C_SMBUS_QUICK))
	{
		return 0;
	}

    switch (size)
    {
    case I2C_SMBUS_QUICK:
    case I2C_SMBUS_BYTE_DATA:
        data->byte = (u8)Inp32(SMBHSTDAT);
        break;
    case I2C_SMBUS_WORD_DATA:
        data->word = Inp32(SMBHSTDAT) + (Inp32(SMBHSTDAT) << 8);
        break;
    case I2C_SMBUS_BLOCK_DATA:
        data->block[0] = (u8)Inp32(SMBHSTDAT);
        if (data->block[0] == 0 || data->block[0] > I2C_SMBUS_BLOCK_MAX)
        {
            return -EPROTO;
        }
        Inp32(SMBBLKSZ);
        for (i = 1; i <= data->block[0]; i++)
        {
            data->block[i] = (u8)Inp32(SMBHSTDAT);
        }
        break;
    }

    return 0;
}

s32 i2c_smbus_nuvoton_nct6793d::i2c_smbus_xfer(u8 addr, char read_write, u8 command, int size, i2c_smbus_data* data)
{
    return nct6793d_access(addr, read_write, command, size, data);
}