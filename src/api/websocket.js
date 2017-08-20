/* global process */
import cluster from 'cluster';
import { isNil } from 'lodash';
import WebSocket from 'uws';
import uuid from 'uuid/v4';
import hash from 'string-hash';
import { URL } from 'url';

import { Rustler, Stream, BannedStream, User } from '../db';
const isValidAdvancedUrl = require('../util/is-valid-advanced-url')(URL);


const debug = require('debug')('overrustle:websocket');

// Services whose channel names are case insensitive. Channel names on these
// services are lowercased in the database in order to prevent duplicates from
// arising due to differences in casing. For example, if one user is watching
// "twitch/Destiny", and another is watching "twitch/destiny", we track this as
// being two users watching "twitch/destiny", since they are both actually
// watching the same stream on this particular service. An example of a
// case-sensitive service would be YouTube.
const CASE_INSENSITIVE_SERVICES = [
  'angelthump',
  'hitbox',
  'twitch',
  'ustream',
];


/**
 * Returns `true` if `service` and `channel` are a valid pair. Otherwise,
 * returns `false`.
 *
 * The validity of a `service`/`channel` pair is determined firstly by the
 * `service`:
 *
 *   * If `service` is "advanced", then the pair is valid if `channel` is a
 *     valid advanced stream URL.
 *   * Else, if `service` is not "advanced", then the pair is valid if `channel`
 *     is between 1 and 64 characters long containing alphanumeric characters,
 *     hyphens, and underscores.
 *
 * @param {string} service
 * @param {string} channel
 */
function isValidServiceChannelPair(service, channel) {
  return (service === 'advanced' && isValidAdvancedUrl(channel))
    || (service !== 'advanced' && /^[a-zA-Z0-9\-_]{1,64}$/.test(channel));
}

/**
 * Returns a `Promise` which resolves to `true` if `service/channel` is a banned
 * stream, or `false` if it is not a banned stream.
 *
 * @param {string} service
 * @param {string channel
 */
async function isBannedStream(service, channel) {
  const [bannedStream] = await BannedStream.findAll({
    where: { channel, service },
    limit: 1,
  });
  return Boolean(bannedStream);
}

class WebSocketServer {
  constructor(options) {
    this.rustlerSockets = new Map();
    this.server = new WebSocket.Server({
      server: options.server,
    });

    this.server.on('connection', this.onConnection.bind(this));
  }

  /**
   * Executed when a new client establishes a connection.
   */
  async onConnection(socket) {
    const id = uuid();
    this.rustlerSockets.set(id, socket);
    await Rustler.create({ id, stream_id: null });

    socket.on('message', this.onMessage.bind(this, id));
    socket.on('close', this.onDisconnect.bind(this, id));
  }

  /**
   * Executed when client with the given `id` disconnects from this server.
   */
  async onDisconnect(id) {
    this.rustlerSockets.delete(id);
    const rustler = await Rustler.findById(id);
    if (!rustler) {
      debug(`rustler ${id} disconnected, but is not in database`);
      return;
    }

    // Get the stream that this rustler was watching, and trigger a recount of
    // the stream's rustlers after deleting the rustler.
    const { stream_id } = rustler;
    await rustler.destroy();
    if (stream_id) {
      await this.updateRustlers(stream_id);
    }
  }

  /**
   * Executed when this server receives a message from a client.
   *
   * @param {string} rustler_id
   * @param {string} message
   */
  async onMessage(rustler_id, message) {
    // An empty message from a client is a ping.
    if (!message) {
      return;
    }

    let event;
    let args;
    try {
      [event, ...args] = JSON.parse(message);
    }
    catch (error) {
      debug('failed to parse received message', message, error);
      return;
    }

    if (event === 'getStream') {
      this.getStream(rustler_id, ...args);
    }
    else if (event === 'setStream') {
      this.setStream(rustler_id, ...args);
    }
    else {
      debug(`unknown request from client: ${event}`);
    }
  }

  /**
   * Sends a message to all rustlers, informing them of the current rustler
   * count for the stream with id `stream_id`.
   */
  async updateRustlers(stream_id) {
    if (!stream_id) {
      return;
    }

    // Get all rustlers currently connected and the number of rustlers watching
    // the stream with id `stream_id` in parallel.
    const [rustlers, rustler_count] = await Promise.all([
      Rustler.findAll({
        where: {
          $or: [
            { stream_id },
            { stream_id: null },
          ],
        },
      }),
      Stream.findRustlersFor(stream_id),
    ]);

    for (const rustler of rustlers) {
      const socket = this.rustlerSockets.get(rustler.id);

      if (socket) {
        socket.send(JSON.stringify(['RUSTLERS_SET', stream_id, rustler_count]));
      }
    }

    // Remove stream if there are no rustlers watching it.
    if (rustler_count === 0) {
      await Stream.destroy({ where: { id: stream_id } });
    }
  }

  /**
   * Gets information about the stream with id `stream_id`, and sends it to the
   * rustler with id `rustler_id`.
   */
  async getStream(rustler_id, stream_id) {
    const socket = this.rustlerSockets.get(rustler_id);
    const stream = await Stream.findById(stream_id);
    const rustlers = await Stream.findRustlersFor(stream.id);
    socket.send(JSON.stringify(['STREAM_GET', {
      ...stream.toJSON(),
      rustlers,
    }]));
  }

  /**
   * Sets rustler's stream to the lobby.
   *
   * @param {string} rustler_id
   */
  async setLobby(rustler_id) {
    const socket = this.rustlerSockets.get(rustler_id);
    const [ rustler ] = await Rustler.findAll({
      where: { id: rustler_id },
      limit: 1,
      include: [
        {
          model: Stream,
          as: 'stream',
        },
      ],
    });

    // Set rustler's stream to `null`.
    await rustler.update({ stream_id: null });

    // Let the rustler know that they've been moved to the lobby.
    socket.send(JSON.stringify(['STREAM_SET', null]));

    // Send the list of streams to the rustler.
    const streams = await Stream.findAllWithRustlers();
    socket.send(JSON.stringify(['STREAMS_SET', streams]));
  }

  /**
   * Sets the stream for rustler with id `rustler_id`.
   *
   * If `channel` and `service` are both nil, then the rustler will be recorded
   * as being in the lobby.
   *
   * @param {string} rustler_id
   * @param {string} channel
   * @param {string} service
   */
  async setStream(rustler_id, channel, service) {
    if (isNil(channel) && isNil(service)) {
      await this.setLobby(rustler_id);
      return;
    }

    const socket = this.rustlerSockets.get(rustler_id);
    const [ rustler ] = await Rustler.findAll({
      where: { id: rustler_id },
      limit: 1,
      include: [
        {
          model: Stream,
          as: 'stream',
        },
      ],
    });

    // Basic channel name sanitization.
    if (!isValidServiceChannelPair(service, channel)) {
      debug(`rejecting ${service}/${channel} as invalid`);
      await this.setLobby(rustler_id);
      return;
    }

    // Encode advanced URLs through punycode.
    if (service === 'advanced' && channel) {
      channel = new URL(channel).href;
    }

    let stream = null;

    // If only `channel` was provided, then use that as the OverRustle username
    // and look up the user's service and channel.
    if (!service) {
      ([ stream ] = await Stream.findAll({
        where: { overrustle_id: channel },
        include: [
          {
            model: User,
            as: 'overrustle',
          },
        ],
        limit: 1,
      }));

      if (!stream) {
        const user = await User.findById(channel);

        // Ensure that `channel` is a real OverRustle user.
        if (!user) {
          debug(`rustler ${rustler_id} attempted to visit stream of unknown user ${channel}`);
          await this.setLobby(rustler_id);
          return;
        }

        // Ensure that the user has a channel
        if (!user.channel || !user.service) {
          await this.setLobby(rustler_id);
          return;
        }

        stream = await Stream.create({
          id: hash(`${user.service}/${user.channel}`),
          channel: user.channel,
          service: user.service,
          overrustle_id: channel,
        });
      }
    }

    // Otherwise, both `service` and `channel` were provided.
    else {
      if (CASE_INSENSITIVE_SERVICES.includes(service)) {
        channel = channel.toLowerCase();
      }

      ([ stream ] = await Stream.findAll({
        where: { channel, service },
        limit: 1,
      }));

      if (!stream) {
        stream = await Stream.create({
          id: hash(`${service}/${channel}`),
          channel,
          service,
        });
      }
    }

    await rustler.update({ stream_id: stream.id });
    const rustlers = await Stream.findRustlersFor(stream.id);

    // Send acknowledgement to the client that their stream has successfully
    // been set.
    socket.send(JSON.stringify(['STREAM_SET', {
      ...stream.toJSON(),
      rustlers,
    }]));

    // Inform everyone else that this rustler's current stream has changed.
    await this.updateRustlers(stream.id);
  }

  /**
   * Sends a message containing the list of current streams to all rustlers.
   */
  async updateLobby() {
    // Get all streams and count rustlers.
    const streams = await Stream.findAllWithRustlers();

    // send `STREAMS_SET`
    for (const socket of this.rustlerSockets.values()) {
      socket.send(JSON.stringify(['STREAMS_SET', streams]));
    }
  }
}

export default function makeWebSocketServer(server) {
  const wss = new WebSocketServer({ server });

  // Need to set up some relaying if we're multi-processing.
  // Set up basic eventing between master and worker.
  if (cluster.isWorker) {
    // Define events to listen to from master.
    const events = {
      updateRustlers: wss.updateRustlers,
      updateLobby: wss.updateLobby.bind(wss),
    };

    // Listen for events from master.
    process.on('message', message => {
      debug('got message from master %j', message);
      if (message.event) {
        const { event, args } = message;
        const handler = events[event];
        if (handler) {
          debug(`handling event from master "${event}"`);
          handler(...(args || []));
        }
      }
    });
  }
}
