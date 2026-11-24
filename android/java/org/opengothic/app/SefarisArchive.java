package org.opengothic.app;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

import me.zhanghai.android.libarchive.Archive;
import me.zhanghai.android.libarchive.ArchiveEntry;
import me.zhanghai.android.libarchive.ArchiveException;

/**
 * Opens the archives that are not ZIP (RAR 4 and 5, 7z, and others) with libarchive (https://github.com/zhanghai/libarchive-android,
 * a dependency of the Gradle build). {@link SefarisInstaller} finds this class by its name, so a build without the library (the
 * script build-apk.sh leaves this file out) still works, only without these archives.
 */
final class SefarisArchive implements SefarisInstaller.Unpacker {
    private static final int BLOCK = 65536;

    @Override
    public void readFile(File file, SefarisInstaller.Sink sink, SpineInstaller.Progress progress) throws IOException {
        read(-1, file, file.length(), sink, progress);
    }

    @Override
    public void readFd(int fd, long size, SefarisInstaller.Sink sink, SpineInstaller.Progress progress) throws IOException {
        read(fd, null, size, sink, progress);
    }

    private static void read(int fd, File file, long size, SefarisInstaller.Sink sink, SpineInstaller.Progress progress) throws IOException {
        long archive;
        try {
            archive = Archive.readNew();
        } catch (ArchiveException e) {
            throw failed(e);
        }
        try {
            Archive.setCharset(archive, "UTF-8".getBytes(StandardCharsets.UTF_8));
            Archive.readSupportFilterAll(archive);
            Archive.readSupportFormatAll(archive);
            if (file != null)
                Archive.readOpenFileName(archive, file.getPath().getBytes(StandardCharsets.UTF_8), BLOCK);
            else
                Archive.readOpenFd(archive, fd, BLOCK);

            ByteBuffer buffer = ByteBuffer.allocate(131072);
            long entry;
            while ((entry = Archive.readNextHeader(archive)) != 0) {
                if (progress.cancelled())
                    throw new SpineInstaller.Cancelled();
                // only the files: the folders are made from the paths of the files, links are of no use
                if (ArchiveEntry.filetype(entry) != ArchiveEntry.AE_IFREG)
                    continue;
                String path = ArchiveEntry.pathnameUtf8(entry);
                if (path == null) {
                    byte[] raw = ArchiveEntry.pathname(entry);
                    if (raw == null)
                        continue;
                    path = new String(raw, StandardCharsets.ISO_8859_1);
                }
                if (ArchiveEntry.isEncrypted(entry))
                    throw new IOException(Texts.t("The archive is protected with a password, the app cannot open it.",
                            "Archiwum jest chronione hasłem, aplikacja go nie otworzy."));
                sink.file(path, new EntryStream(archive, buffer, size, progress));
            }
        } catch (ArchiveException e) {
            throw failed(e);
        } finally {
            try {
                Archive.free(archive);
            } catch (ArchiveException ignored) {
                // nothing is left to do with it
            }
        }
    }

    private static IOException failed(ArchiveException e) {
        return new IOException(Texts.t("The archive cannot be read: ", "Nie można odczytać archiwum: ")
                + (e.getMessage() != null ? e.getMessage() : "code " + e.getCode()), e);
    }

    /** The data of the file that libarchive is at now. It ends with the file; what is not read is skipped by the next header. */
    private static final class EntryStream extends InputStream {
        private final long archive;
        private final ByteBuffer buffer;
        private final long size;
        private final SpineInstaller.Progress progress;
        private int position;
        private int limit;

        EntryStream(long archive, ByteBuffer buffer, long size, SpineInstaller.Progress progress) {
            this.archive = archive;
            this.buffer = buffer;
            this.size = size;
            this.progress = progress;
        }

        @Override
        public int read() throws IOException {
            byte[] one = new byte[1];
            return read(one, 0, 1) < 0 ? -1 : one[0] & 0xff;
        }

        @Override
        public int read(byte[] b, int off, int len) throws IOException {
            if (len == 0)
                return 0;
            if (position == limit) {
                if (progress.cancelled())
                    throw new SpineInstaller.Cancelled();
                buffer.clear();
                try {
                    Archive.readData(archive, buffer);
                    // the position of the buffer is the number of the bytes that were read, 0 at the end of the file
                    limit = buffer.position();
                    progress.update(Archive.filterBytes(archive, 0), size, SefarisInstaller.UNPACK);
                } catch (ArchiveException e) {
                    throw failed(e);
                }
                position = 0;
                if (limit == 0)
                    return -1;
            }
            int n = Math.min(len, limit - position);
            System.arraycopy(buffer.array(), position, b, off, n);
            position += n;
            return n;
        }
    }
}
